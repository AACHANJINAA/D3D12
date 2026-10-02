#pragma once
#include <shellapi.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <stdexcept>

// Included after the project's viewer/scene types; both executables consume this file.
class BENCHMARK_SCENE
{
public:
    bool isenabled = false;
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
        try
        {
            if (path.empty()) throw std::runtime_error("--benchmark requires a scene JSON path.");
            path = std::filesystem::absolute(path);
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Could not open benchmark scene JSON.");
            const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (source.size() > 1024 * 1024) throw std::runtime_error("Benchmark scene is too large.");
            const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(source));
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
            const auto entries = json.GetNamedArray(L"objects");
            if (entries.Size() == 0 || entries.Size() > SCENE::object_limit)
                throw std::runtime_error("Benchmark requires 1 to 128 objects.");
            for (const auto& entry : entries)
            {
                const auto object = entry.GetObject();
                VIEWER_REQUEST request;
                request.action = VIEWER_ACTION::add;
                request.path = path.parent_path() / object.GetNamedString(L"model").c_str();
                request.position = vector(object, L"position", {});
                request.rotation = vector(object, L"rotation", {});
                request.scale = vector(object, L"scale", {1,1,1});
                if (!request.has_valid_transform()) throw std::runtime_error("Invalid benchmark transform.");
                if (std::abs(request.scale.x - request.scale.y) > 0.00001f || std::abs(request.scale.x - request.scale.z) > 0.00001f)
                    throw std::runtime_error("PIP baseline requires uniform positive object scale.");
                objects.push_back(std::move(request));
            }
            isenabled = true;
            return true;
        }
        catch (const winrt::hresult_error& exception) { error = winrt::to_string(exception.message()); }
        catch (const std::exception& exception) { error = exception.what(); }
        return false;
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
};
