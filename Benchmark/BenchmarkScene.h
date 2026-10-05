#pragma once
#include <shellapi.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <stdexcept>
#include <unordered_map>
#include "BenchmarkLights.h"

// Included after the project's viewer/scene types; both executables consume this file.
class BENCHMARK_SCENE
{
public:
    bool isenabled = false;
    bool isinstancing = true;
    bool isautomated = false, ismatched = true, iskeep_open = false;
    double warmup_seconds = 10, measurement_seconds = 0;
    std::filesystem::path output_directory;
    std::string run_id;
    BENCHMARK_LIGHT_DATA light_data;
    BENCHMARK_UI_STATE ui;
    std::string source_json;
    UINT width = 1920, height = 1080, vsync = 0;
    MATH::VECTOR3 camera_position{ 0, 1, -8 }, camera_target{};
    MATH::VECTOR3 light_direction{ -0.3f, -0.8f, 0.4f }, light_color{ 1, 1, 1 };
    float light_intensity = 0.8f;
    std::vector<VIEWER_REQUEST> objects;

    bool load_arguments(std::string& error)
    {
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments) { error = "Could not read command line."; return false; }
        std::filesystem::path path;
        bool isrequested = false;
        for (int index = 1; index < count; ++index)
            if (std::wstring(arguments[index]) == L"--benchmark")
            {
                isrequested = true;
                if (index + 1 < count) path = arguments[++index];
            }
        LocalFree(arguments);
        if (!isrequested) return true;
        return load_file(path, error);
    }

    bool load_file(std::filesystem::path path, std::string& error)
    {
        objects.clear();
        isenabled = false;
        _isreported = false;
        try
        {
            if (path.empty()) throw std::runtime_error("--benchmark requires a scene JSON path.");
            path = std::filesystem::absolute(path);
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Could not open benchmark scene JSON.");
            const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (source.size() > 1024 * 1024) throw std::runtime_error("Benchmark scene is too large.");
            const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(source));
            source_json = source;
            _source_path = path;
            isinstancing = json.GetNamedBoolean(L"instancing", true);
            for (const auto* key : { L"frustum_culling", L"distance_culling", L"occlusion_culling", L"backface_culling" })
                if (json.GetNamedBoolean(key, false))
                    throw std::runtime_error("Culling is disabled in batching benchmarks.");
            auto integer = [&](const wchar_t* name, UINT fallback, UINT minimum, UINT maximum)
            {
                const double value = json.GetNamedNumber(name, fallback);
                if (!std::isfinite(value) || value != std::floor(value) || value < minimum || value > maximum)
                    throw std::runtime_error("Invalid benchmark integer setting.");
                return static_cast<UINT>(value);
            };
            width = integer(L"width", width, 920, 7680);
            height = integer(L"height", height, 640, 4320);
            vsync = integer(L"vsync", vsync, 0, 1);
            const auto profile = json.GetNamedString(L"profile", L"matched_quality");
            if (profile != L"matched_quality" && profile != L"native") throw std::runtime_error("Unknown comparison profile.");
            ismatched = profile == L"matched_quality";
            warmup_seconds = json.GetNamedNumber(L"warmup_seconds", 10);
            measurement_seconds = json.GetNamedNumber(L"measurement_seconds", 0);
            if (!std::isfinite(warmup_seconds) || !std::isfinite(measurement_seconds) || warmup_seconds < 0 ||
                warmup_seconds > 300 || measurement_seconds < 0 || measurement_seconds > 600)
                throw std::runtime_error("Invalid benchmark duration (maximum 300s warmup, 600s measurement).");
            isautomated = measurement_seconds > 0;
            iskeep_open = json.GetNamedBoolean(L"keep_open", false);
            if (isautomated && !ismatched) throw std::runtime_error("Timed comparisons require matched_quality.");
            output_directory = path.parent_path() / json.GetNamedString(L"output_directory", L"results").c_str();
            run_id = winrt::to_string(json.GetNamedString(L"run_id", L"run"));
            if (run_id.empty() || run_id.size() > 100 || run_id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string::npos)
                throw std::runtime_error("run_id must contain only letters, digits, dash or underscore.");
            auto vector = [](const auto& object, const wchar_t* key, MATH::VECTOR3 fallback)
            {
                if (!object.HasKey(key)) return fallback;
                const auto values = object.GetNamedArray(key);
                if (values.Size() != 3) throw std::runtime_error("Expected a 3-component benchmark vector.");
                MATH::VECTOR3 result{ static_cast<float>(values.GetNumberAt(0)),
                    static_cast<float>(values.GetNumberAt(1)), static_cast<float>(values.GetNumberAt(2)) };
                if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
                    throw std::runtime_error("Non-finite benchmark vector.");
                return result;
            };
            camera_position = vector(json, L"camera_position", camera_position);
            camera_target = vector(json, L"camera_target", camera_target);
            const auto direction = MATH::subtract(camera_target, camera_position);
            if (MATH::length(MATH::cross(direction, {0,1,0})) < 0.0001f)
                throw std::runtime_error("Invalid benchmark camera direction.");
            light_direction = vector(json, L"light_direction", light_direction);
            if (MATH::length(light_direction) < 0.0001f) throw std::runtime_error("Invalid light direction.");
            light_color = vector(json, L"light_color", light_color);
            light_intensity = static_cast<float>(json.GetNamedNumber(L"light_intensity", light_intensity));
            if (!std::isfinite(light_intensity) || light_intensity < 0 || light_color.x < 0 || light_color.y < 0 || light_color.z < 0)
                throw std::runtime_error("Invalid light intensity/color.");
            auto append = [&](VIEWER_REQUEST request)
            {
                if (objects.size() >= SCENE::benchmark_object_limit)
                    throw std::runtime_error("Benchmark object limit is 50000.");
                if (!request.has_valid_transform()) throw std::runtime_error("Invalid benchmark transform.");
                if (std::abs(request.scale.x - request.scale.y) > 0.00001f || std::abs(request.scale.x - request.scale.z) > 0.00001f)
                    throw std::runtime_error("PIP baseline requires uniform positive object scale.");
                objects.push_back(std::move(request));
            };
            if (json.HasKey(L"objects")) for (const auto& entry : json.GetNamedArray(L"objects"))
            {
                const auto object = entry.GetObject();
                VIEWER_REQUEST request;
                request.action = VIEWER_ACTION::add;
                request.path = path.parent_path() / object.GetNamedString(L"model").c_str();
                request.position = vector(object, L"position", {});
                request.rotation = vector(object, L"rotation", {});
                request.scale = vector(object, L"scale", {1,1,1});
                append(std::move(request));
            }
            if (json.HasKey(L"grid"))
            {
                const auto grid = json.GetNamedObject(L"grid");
                auto grid_integer = [&](const wchar_t* key)
                {
                    const double value = grid.GetNamedNumber(key);
                    if (!std::isfinite(value) || value != std::floor(value) || value < 1 || value > SCENE::benchmark_object_limit)
                        throw std::runtime_error("Invalid grid count/columns.");
                    return static_cast<UINT>(value);
                };
                const UINT count = grid_integer(L"count"), columns = grid_integer(L"columns");
                const auto origin = vector(grid, L"origin", {});
                const auto spacing = vector(grid, L"spacing", {2,2,0});
                VIEWER_REQUEST request;
                request.action = VIEWER_ACTION::add;
                request.path = path.parent_path() / grid.GetNamedString(L"model").c_str();
                request.rotation = vector(grid, L"rotation", {});
                request.scale = vector(grid, L"scale", {1,1,1});
                for (UINT index = 0; index < count; ++index)
                {
                    request.position = {origin.x + (index % columns) * spacing.x,
                        origin.y + (index / columns) * spacing.y,
                        origin.z + (index / columns) * spacing.z};
                    append(request);
                }
            }
            if (objects.empty()) throw std::runtime_error("Benchmark requires objects or a grid.");
            light_data = {};
            light_data.ismatched = ismatched ? 1 : 0;
            light_data.count = integer(L"light_count", 0, 0, 256);
            light_data.isdirectional = json.GetNamedBoolean(L"directional_light", light_data.count == 0) ? 1 : 0;
            if (!ismatched && light_data.count != 0) throw std::runtime_error("Point lights require matched_quality.");
            const auto center = vector(json, L"light_center", {0,0,-4});
            const auto extent = vector(json, L"light_extent", {6,6,2});
            const float radius = static_cast<float>(json.GetNamedNumber(L"light_radius", 15));
            const float power = static_cast<float>(json.GetNamedNumber(L"point_intensity", 30));
            if (!std::isfinite(radius) || radius <= 0 || radius > 100000 || !std::isfinite(power) || power < 0 || power > 1000000 ||
                extent.x < 0 || extent.y < 0 || extent.z < 0) throw std::runtime_error("Invalid point light volume/intensity.");
            UINT seed = integer(L"seed", 12345, 1, UINT_MAX);
            auto random = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
                return static_cast<float>(seed & 0xffffff) / 16777215.0f * 2 - 1; };
            for (auto& point : light_data.points)
            {
                point.position_radius = {center.x + extent.x * random(), center.y + extent.y * random(),
                    center.z + extent.z * random(), radius};
                point.radiance = {light_color.x * power, light_color.y * power, light_color.z * power, 0};
            }
            ui = {};
            ui.light_count = static_cast<int>(light_data.count);
            ui.isinstancing = isinstancing;
            ui.islocked = isautomated;
            ui.objects = objects.size();
            ui.profile = ismatched ? "matched_quality" : "native";
            ui.study = static_cast<int>(integer(L"study", 0, 0, 2));
            ui.workload = static_cast<int>(integer(L"workload", 0, 0, 1));
            ui.iscached = json.GetNamedBoolean(L"descriptor_cache", true);
            ui.iscompact = json.GetNamedBoolean(L"compact_gbuffer", false);
            ui.current_label = winrt::to_string(json.GetNamedString(L"label", L"B"));
            ui.width = width; ui.height = height;
            if (json.HasKey(L"reference_result"))
            {
                const auto reference_path = path.parent_path() / json.GetNamedString(L"reference_result").c_str();
                std::ifstream reference_file(reference_path, std::ios::binary);
                if (!reference_file) throw std::runtime_error("Comparison reference result is missing.");
                const std::string text((std::istreambuf_iterator<char>(reference_file)), {});
                const auto reference = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(text));
                if (reference.GetNamedString(L"status") != L"complete")
                    throw std::runtime_error("Reference measurement is incomplete.");
                const auto input = reference.GetNamedObject(L"scene");
                if (input.GetNamedNumber(L"width") != width || input.GetNamedNumber(L"height") != height ||
                    input.GetNamedNumber(L"light_count", 0) != light_data.count ||
                    input.GetNamedNumber(L"workload", 0) != ui.workload ||
                    input.GetNamedObject(L"grid").GetNamedNumber(L"count") != objects.size())
                    throw std::runtime_error("Reference scene conditions do not match.");
                const auto summary = reference.GetNamedObject(L"summary");
                auto mean = [&](const wchar_t* key) { return summary.HasKey(key) ? summary.GetNamedObject(key).GetNamedNumber(L"mean") : 0.0; };
                ui.reference = {mean(L"prepare_cpu_ms"), mean(L"record_cpu_ms"), mean(L"gpu_ms"),
                    mean(L"mesh_gpu_ms"), mean(L"lighting_gpu_ms"), static_cast<size_t>(mean(L"mesh_draw_calls")),
                    static_cast<size_t>(mean(L"srv_writes")), static_cast<size_t>(reference.GetNamedNumber(L"frames")),
                    mean(L"gbuffer_payload_mib")};
                ui.reference_label = winrt::to_string(input.GetNamedString(L"label", reference.GetNamedString(L"renderer")));
                ui.reference_renderer = winrt::to_string(reference.GetNamedString(L"renderer"));
                ui.isreference = true;
            }
            isenabled = true;
            return true;
        }
        catch (const winrt::hresult_error& exception) { error = winrt::to_string(exception.message()); }
        catch (const std::exception& exception) { error = exception.what(); }
        return false;
    }

    template<class LOADER>
    bool populate(SCENE& scene, LOADER load_model, std::string& error) const
    {
        try
        {
            std::unordered_map<std::wstring, std::shared_ptr<MODEL_RESOURCE>> models;
            std::vector<SCENE_OBJECT> generated;
            generated.reserve(objects.size());
            for (const auto& request : objects)
            {
                const auto key = request.path.lexically_normal().wstring();
                auto found = models.find(key);
                if (found == models.end())
                {
                    auto model = load_model(request);
                    if (!model) throw std::runtime_error("Benchmark model import failed.");
                    found = models.emplace(key, std::move(model)).first;
                }
                SCENE_OBJECT object;
                object.model = found->second;
                object.position = request.position;
                object.rotation = request.rotation;
                object.scale = request.scale;
                generated.push_back(std::move(object));
            }
            scene.set_benchmark_objects(std::move(generated));
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    void apply_workload(SCENE& scene) const
    {
        auto& items = scene.get_objects();
        const size_t layer_size = (std::max)(size_t{1}, (objects.size() + 19) / 20);
        const size_t columns = static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(layer_size) * 1.5)));
        const size_t rows = (layer_size + columns - 1) / columns;
        for (size_t index = 0; index < items.size(); ++index)
        {
            auto& object = items[index];
            if (ui.workload == 0)
            {
                object.position = objects[index].position;
                object.scale = objects[index].scale;
            }
            else
            {
                const size_t cell = index % layer_size;
                const float spacing = 24.0f / static_cast<float>(columns);
                object.position = {(static_cast<float>(cell % columns) - (columns - 1) * .5f) * spacing,
                    (static_cast<float>(cell / columns) - (rows - 1) * .5f) * spacing,
                    8.0f - static_cast<float>(index / layer_size) * .4f};
                object.scale = {spacing * .95f, spacing * .95f, .2f};
            }
        }
    }

    bool report_submission(const char* renderer, const SCENE& scene, size_t draws, size_t instances)
    {
        if (!isenabled || _isreported) return true;
        try
        {
            size_t expected = 0;
            for (const auto& object : scene.get_objects()) expected += object.model->get_mesh().get_primitives().size();
            if (instances != expected || (expected != 0 && draws == 0)) return false;
            using namespace winrt::Windows::Data::Json;
            JsonObject report;
            report.SetNamedValue(L"renderer", JsonValue::CreateStringValue(winrt::to_hstring(renderer)));
            report.SetNamedValue(L"status", JsonValue::CreateStringValue(L"cpu_submission_validated_not_gpu_verified"));
            report.SetNamedValue(L"instancing", JsonValue::CreateBooleanValue(isinstancing));
            report.SetNamedValue(L"object_culling", JsonValue::CreateBooleanValue(false));
            report.SetNamedValue(L"backface_culling", JsonValue::CreateBooleanValue(false));
            report.SetNamedValue(L"objects", JsonValue::CreateNumberValue(static_cast<double>(scene.get_objects().size())));
            report.SetNamedValue(L"primitive_instances", JsonValue::CreateNumberValue(static_cast<double>(instances)));
            report.SetNamedValue(L"mesh_draw_calls", JsonValue::CreateNumberValue(static_cast<double>(draws)));
            report.SetNamedValue(L"submitted_triangles", JsonValue::CreateNumberValue(static_cast<double>(scene.triangle_count())));
            const auto directory = _source_path.parent_path() / L"results";
            std::filesystem::create_directories(directory);
            const auto name = std::string(renderer) + "-" + _source_path.stem().string() +
                (isinstancing ? "-batched.json" : "-unbatched.json");
            std::ofstream file(directory / name, std::ios::binary);
            file << winrt::to_string(report.Stringify());
            if (!file) return false;
            _isreported = true;
            return true;
        }
        catch (...) { return false; }
    }

    void apply(VIEWER_SETTINGS& settings) const
    {
        settings.light_direction = light_direction;
        settings.light_color = light_color;
        settings.light_intensity = light_intensity;
        settings.is_light_orbiting = false;
        settings.exposure = 0;
        settings.environment_intensity = 1;
        settings.mode = VIEW_MODE::lit;
        settings.is_skybox_visible = true;
    }
private:
    std::filesystem::path _source_path;
    bool _isreported = false;
};
