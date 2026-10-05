#pragma once
#include "BenchmarkScene.h"
#include <iomanip>
#include <numeric>
#include <sstream>

struct FRAME_MEASUREMENTS
{
    UINT64 frame_id = 0;
    double frame_ms = 0, frame_interval_ms = 0, prepare_ms = 0, record_ms = 0;
    double submit_ms = 0, present_ms = 0, sync_ms = 0;
    double gpu_ms = 0, mesh_gpu_ms = 0, lighting_gpu_ms = 0, sky_gpu_ms = 0, ui_gpu_ms = 0;
    size_t draws = 0, instances = 0, triangles = 0, srv_writes = 0;
    double gbuffer_mib = 0;
    UINT lights = 0;
    bool ismeasured = false;
};

class FRAME_PROFILER
{
public:
    static constexpr UINT slot_count = 2, query_count = 10;
    static double now()
    {
        static const double frequency = [] { LARGE_INTEGER value; QueryPerformanceFrequency(&value); return static_cast<double>(value.QuadPart); }();
        LARGE_INTEGER value; QueryPerformanceCounter(&value);
        return static_cast<double>(value.QuadPart) / frequency;
    }
    ~FRAME_PROFILER() { if (_readback && _mapped) _readback->Unmap(0, nullptr); }

    bool initialize(ID3D12Device* device, ID3D12CommandQueue* queue, IDXGIFactory4* factory,
        const BENCHMARK_SCENE& scene, const char* renderer) try
    {
        if (!scene.isenabled) return true;
        device->QueryInterface(IID_PPV_ARGS(&_diagnostics));
        _renderer = renderer;
        _isforward = _renderer == "Forward";
        _isautomated = scene.isautomated;
        _warmup = scene.warmup_seconds;
        _duration = scene.measurement_seconds;
        D3D12_QUERY_HEAP_DESC description{};
        description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        description.Count = slot_count * query_count;
        if (FAILED(queue->GetTimestampFrequency(&_frequency)) || !_frequency ||
            FAILED(device->CreateQueryHeap(&description, IID_PPV_ARGS(&_queries)))) return false;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
        const auto resource = CD3DX12_RESOURCE_DESC::buffer(slot_count * query_count * sizeof(UINT64));
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_readback)))) return false;
        const D3D12_RANGE read{0, slot_count * query_count * sizeof(UINT64)};
        if (FAILED(_readback->Map(0, &read, reinterpret_cast<void**>(&_mapped)))) return false;
        using namespace winrt::Windows::Data::Json;
        _metadata = JsonObject();
        auto string = [&](const wchar_t* name, const std::string& value) { _metadata.SetNamedValue(name, JsonValue::CreateStringValue(winrt::to_hstring(value))); };
        string(L"renderer", renderer);
        string(L"profile", scene.ismatched ? "matched_quality" : "native");
        string(L"quality_status", "shared_lighting_equations_gbuffer_quantization_differs_not_pixel_certified");
        string(L"cpu_scope", "frame=post-UI render preparation through slot wait; interval=between preparations including UI; prepare=batching/counts/light upload; record=allocator reset through close; submit=ExecuteCommandLists; sync=signal and slot wait");
        string(L"gpu_scope", "timestamp total includes clear/barriers/mesh/sky/light/comparison UI; excludes Present and query resolve");
        _metadata.SetNamedValue(L"scene", JsonObject::Parse(winrt::to_hstring(scene.source_json)));
        _metadata.SetNamedValue(L"frames_in_flight", JsonValue::CreateNumberValue(2));
        _metadata.SetNamedValue(L"timestamp_frequency", JsonValue::CreateNumberValue(static_cast<double>(_frequency)));
        _metadata.SetNamedValue(L"fps_ui", JsonValue::CreateBooleanValue(true));
#if defined(_DEBUG)
        string(L"build", "Debug");
#else
        string(L"build", "Release");
#endif
        ComPtr<IDXGIAdapter1> adapter;
        if (SUCCEEDED(factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter))))
        {
            DXGI_ADAPTER_DESC1 gpu{};
            adapter->GetDesc1(&gpu);
            string(L"gpu", winrt::to_string(gpu.Description));
            _metadata.SetNamedValue(L"vendor_id", JsonValue::CreateNumberValue(gpu.VendorId));
            _metadata.SetNamedValue(L"device_id", JsonValue::CreateNumberValue(gpu.DeviceId));
            LARGE_INTEGER version{};
            if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
                string(L"driver_version_raw", std::to_string(version.QuadPart));
            adapter.As(&_adapter);
        }
        if (_isautomated)
        {
            std::filesystem::create_directories(scene.output_directory);
            _path = scene.output_directory / (scene.run_id + "-" + renderer);
            if (std::filesystem::exists(_path.string() + ".json") || std::filesystem::exists(_path.string() + ".csv")) return false;
            _samples.reserve(32768);
        }
        _isenabled = true;
        return true;
    }
    catch (...) { return false; }

    bool begin(UINT slot, ID3D12Fence* fence)
    {
        if (!_isenabled) return true;
        if (!collect(slot, fence)) return false;
        const double time = now();
        if (_start == 0) _start = time;
        auto& frame = _slots.at(slot);
        frame.data = {};
        frame.data.frame_id = _next_frame++;
        frame.data.frame_interval_ms = _previous == 0 ? 0 : (time - _previous) * 1000;
        frame.data.ismeasured = _isautomated && time - _start >= _warmup && time - _start < _warmup + _duration;
        frame.start = time;
        _previous = time;
        return true;
    }
    void stamp(ID3D12GraphicsCommandList* list, UINT slot, UINT index)
    {
        if (_isenabled) list->EndQuery(_queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * query_count + index);
    }
    void resolve(ID3D12GraphicsCommandList* list, UINT slot)
    {
        if (_isenabled) list->ResolveQueryData(_queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            slot * query_count, query_count, _readback.Get(), slot * query_count * sizeof(UINT64));
    }
    void submit(UINT slot, UINT64 fence, const FRAME_MEASUREMENTS& data)
    {
        if (!_isenabled) return;
        auto& frame = _slots.at(slot);
        const auto id = frame.data.frame_id;
        const auto interval = frame.data.frame_interval_ms;
        const bool ismeasured = frame.data.ismeasured;
        frame.data = data;
        frame.data.frame_id = id;
        frame.data.frame_interval_ms = interval;
        frame.data.ismeasured = ismeasured;
        frame.data.frame_ms = (now() - frame.start) * 1000;
        frame.fence = fence;
        frame.ispending = true;
    }
    bool is_finished() const { return _isenabled && !_isfinished && _isautomated && _start != 0 && now() - _start >= _warmup + _duration; }
    void update_ui(BENCHMARK_UI_STATE& ui)
    {
        ui.current = _published;
        ui.istimed = _isfinished && _isautomated;
    }
    const FRAME_MEASUREMENTS& latest() const { return _latest; }
    bool finish(ID3D12Fence* fence, bool issuccess)
    {
        if (!_isenabled || _isfinished) return true;
        for (UINT slot = 0; slot < slot_count; ++slot) if (!collect(slot, fence)) issuccess = false;
        _isfinished = true;
        if (!_isautomated) return true;
        using namespace winrt::Windows::Data::Json;
        UINT64 errors = 0;
        if (_diagnostics)
            for (UINT64 index = 0; index < _diagnostics->GetNumStoredMessages(); ++index)
            {
                SIZE_T size = 0;
                _diagnostics->GetMessage(index, nullptr, &size);
                std::vector<char> bytes(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
                if (SUCCEEDED(_diagnostics->GetMessage(index, message, &size)) &&
                    message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    ++errors;
                    std::fputs(message->pDescription, stderr);
                }
            }
        if (errors) issuccess = false;
        _metadata.SetNamedValue(L"debug_layer_available", JsonValue::CreateBooleanValue(_diagnostics != nullptr));
        _metadata.SetNamedValue(L"debug_errors", JsonValue::CreateNumberValue(static_cast<double>(errors)));
        if (_samples.empty()) issuccess = false;
        std::sort(_samples.begin(), _samples.end(), [](const auto& a, const auto& b) { return a.frame_id < b.frame_id; });
        std::ofstream csv(_path.string() + ".csv");
        csv << "frame_id,frame_ms,frame_interval_ms,prepare_cpu_ms,record_cpu_ms,submit_cpu_ms,present_cpu_ms,sync_cpu_ms,gpu_ms,mesh_gpu_ms,lighting_gpu_ms,sky_gpu_ms,ui_gpu_ms,mesh_draw_calls,primitive_instances,triangles,point_lights,queue_submissions,srv_writes,gbuffer_payload_mib\n";
        csv << std::fixed << std::setprecision(6);
        for (const auto& sample : _samples)
        {
            csv << sample.frame_id << ',' << sample.frame_ms << ',' << sample.frame_interval_ms << ',' << sample.prepare_ms << ',' << sample.record_ms << ','
                << sample.submit_ms << ',' << sample.present_ms << ',' << sample.sync_ms << ',' << sample.gpu_ms << ',' << sample.mesh_gpu_ms << ',';
            if (!_isforward) csv << sample.lighting_gpu_ms;
            csv << ',' << sample.sky_gpu_ms << ',' << sample.ui_gpu_ms << ',' << sample.draws << ',' << sample.instances << ','
                << sample.triangles << ',' << sample.lights << ",1," << sample.srv_writes << ',' << sample.gbuffer_mib << '\n';
        }
        csv.flush();
        if (!csv) issuccess = false;
        _metadata.SetNamedValue(L"status", JsonValue::CreateStringValue(issuccess ? L"complete" : L"failed_or_interrupted"));
        _metadata.SetNamedValue(L"frames", JsonValue::CreateNumberValue(static_cast<double>(_samples.size())));
        JsonObject summary;
        auto metric = [&](const wchar_t* name, auto getter)
        {
            if (_samples.empty()) return;
            std::vector<double> values;
            for (const auto& sample : _samples) values.push_back(getter(sample));
            std::sort(values.begin(), values.end());
            JsonObject stats;
            stats.SetNamedValue(L"mean", JsonValue::CreateNumberValue(std::accumulate(values.begin(), values.end(), 0.0) / values.size()));
            auto quantile = [&](double q) { return values[static_cast<size_t>(std::ceil(q * values.size())) - 1]; };
            stats.SetNamedValue(L"median", JsonValue::CreateNumberValue(quantile(.5)));
            stats.SetNamedValue(L"p95", JsonValue::CreateNumberValue(quantile(.95)));
            stats.SetNamedValue(L"p99", JsonValue::CreateNumberValue(quantile(.99)));
            summary.SetNamedValue(name, stats);
        };
        metric(L"frame_ms", [](const auto& s) { return s.frame_ms; });
        metric(L"frame_interval_ms", [](const auto& s) { return s.frame_interval_ms; });
        metric(L"prepare_cpu_ms", [](const auto& s) { return s.prepare_ms; });
        metric(L"record_cpu_ms", [](const auto& s) { return s.record_ms; });
        metric(L"submit_cpu_ms", [](const auto& s) { return s.submit_ms; });
        metric(L"sync_cpu_ms", [](const auto& s) { return s.sync_ms; });
        metric(L"present_cpu_ms", [](const auto& s) { return s.present_ms; });
        metric(L"gpu_ms", [](const auto& s) { return s.gpu_ms; });
        metric(L"mesh_gpu_ms", [](const auto& s) { return s.mesh_gpu_ms; });
        if (!_isforward) metric(L"lighting_gpu_ms", [](const auto& s) { return s.lighting_gpu_ms; });
        metric(L"ui_gpu_ms", [](const auto& s) { return s.ui_gpu_ms; });
        metric(L"sky_gpu_ms", [](const auto& s) { return s.sky_gpu_ms; });
        metric(L"mesh_draw_calls", [](const auto& s) { return static_cast<double>(s.draws); });
        metric(L"primitive_instances", [](const auto& s) { return static_cast<double>(s.instances); });
        metric(L"srv_writes", [](const auto& s) { return static_cast<double>(s.srv_writes); });
        metric(L"gbuffer_payload_mib", [](const auto& s) { return s.gbuffer_mib; });
        if (!_samples.empty())
        {
            _published = {};
            for (const auto& sample : _samples) accumulate(_published, sample);
            average(_published);
        }
        _metadata.SetNamedValue(L"summary", summary);
        if (_adapter)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO info{};
            if (SUCCEEDED(_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
                _metadata.SetNamedValue(L"video_memory_bytes_at_end", JsonValue::CreateNumberValue(static_cast<double>(info.CurrentUsage)));
        }
        std::ofstream metadata(_path.string() + ".json");
        metadata << winrt::to_string(_metadata.Stringify());
        metadata.flush();
        return issuccess && static_cast<bool>(metadata);
    }
private:
    struct SLOT { FRAME_MEASUREMENTS data; double start = 0; UINT64 fence = 0; bool ispending = false; };
    bool collect(UINT slot, ID3D12Fence* fence)
    {
        auto& frame = _slots.at(slot);
        if (!frame.ispending) return true;
        if (!fence || fence->GetCompletedValue() == UINT64_MAX || fence->GetCompletedValue() < frame.fence) return false;
        const auto* ticks = _mapped + slot * query_count;
        for (UINT index = 0; index < query_count; index += 2) if (ticks[index + 1] < ticks[index]) return false;
        auto milliseconds = [&](UINT first) { return static_cast<double>(ticks[first + 1] - ticks[first]) * 1000.0 / _frequency; };
        frame.data.gpu_ms = milliseconds(0);
        frame.data.mesh_gpu_ms = milliseconds(2);
        frame.data.sky_gpu_ms = milliseconds(4);
        frame.data.lighting_gpu_ms = milliseconds(6);
        frame.data.ui_gpu_ms = milliseconds(8);
        _latest = frame.data;
        if (!_isfinished)
        {
            accumulate(_rolling, frame.data);
            if (now() - _publish_time >= .5)
            {
                _published = _rolling;
                average(_published);
                _rolling = {};
                _publish_time = now();
            }
        }
        if (frame.data.ismeasured)
        {
            if (_samples.size() >= 2000000) return false;
            _samples.push_back(frame.data);
        }
        frame.ispending = false;
        return true;
    }
    bool _isenabled = false, _isautomated = false, _isforward = false, _isfinished = false;
    static void accumulate(COMPARISON_VALUES& sum, const FRAME_MEASUREMENTS& frame)
    {
        sum.prepare += frame.prepare_ms; sum.record += frame.record_ms; sum.gpu += frame.gpu_ms;
        sum.mesh += frame.mesh_gpu_ms; sum.light += frame.lighting_gpu_ms;
        sum.draws += frame.draws; sum.srvs += frame.srv_writes; sum.gbuffer_mib += frame.gbuffer_mib; ++sum.frames;
    }
    static void average(COMPARISON_VALUES& sum)
    {
        if (!sum.frames) return;
        const double count = static_cast<double>(sum.frames);
        sum.prepare /= count; sum.record /= count; sum.gpu /= count; sum.mesh /= count;
        sum.light /= count; sum.gbuffer_mib /= count; sum.draws /= sum.frames; sum.srvs /= sum.frames;
    }
    COMPARISON_VALUES _rolling{}, _published{};
    double _publish_time = 0;
    double _start = 0, _previous = 0, _warmup = 0, _duration = 0;
    UINT64 _frequency = 0, _next_frame = 0;
    ComPtr<ID3D12QueryHeap> _queries;
    ComPtr<ID3D12Resource> _readback;
    ComPtr<IDXGIAdapter3> _adapter;
    ComPtr<ID3D12InfoQueue> _diagnostics;
    UINT64* _mapped = nullptr;
    std::array<SLOT, slot_count> _slots{};
    std::vector<FRAME_MEASUREMENTS> _samples;
    FRAME_MEASUREMENTS _latest;
    std::filesystem::path _path;
    std::string _renderer;
    winrt::Windows::Data::Json::JsonObject _metadata{nullptr};
};
