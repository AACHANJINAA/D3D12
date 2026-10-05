#pragma once
#include <array>

struct BENCHMARK_POINT_LIGHT
{
    MATH::VECTOR4 position_radius{};
    MATH::VECTOR4 radiance{};
};

struct BENCHMARK_LIGHT_DATA
{
    UINT count = 0, ismatched = 0, isdirectional = 1;
    float environment = 1;
    std::array<BENCHMARK_POINT_LIGHT, 256> points{};
    UINT iscompact = 0;
    float viewport_width = 1, viewport_height = 1, padding = 0;
    MATH::MATRIX4X4 inverse_view_projection{};
};
static_assert(sizeof(BENCHMARK_POINT_LIGHT) == 32 && sizeof(BENCHMARK_LIGHT_DATA) == 8288);

struct COMPARISON_VALUES
{
    double prepare = 0, record = 0, gpu = 0, mesh = 0, light = 0;
    size_t draws = 0, srvs = 0, frames = 0;
    double gbuffer_mib = 0;
};

struct BENCHMARK_UI_STATE
{
    int light_count = 0;
    bool isinstancing = true, islocked = false;
    size_t objects = 0, draws = 0;
    double prepare_ms = 0, record_ms = 0, gpu_ms = 0, mesh_ms = 0, lighting_ms = 0;
    const char* renderer = "";
    const char* profile = "matched_quality";
    int study = 0, workload = 0, view = 0;
    bool iscached = true, iscompact = false, isreference = false, istimed = false;
    COMPARISON_VALUES current{}, reference{};
    std::string reference_label = "A", current_label = "B";
    std::string reference_renderer;
    UINT width = 1280, height = 720;
};

// Updated only after the renderer has waited for the corresponding frame slot.
class BENCHMARK_LIGHT_BUFFER
{
public:
    bool initialize(ID3D12Device* device, const BENCHMARK_LIGHT_DATA& data)
    {
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        const auto description = CD3DX12_RESOURCE_DESC::buffer((sizeof(data) + 255) & ~size_t{255});
        for (auto& buffer : _buffers)
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)))) return false;
        for (UINT slot = 0; slot < 2; ++slot) if (!update(slot, data)) return false;
        return true;
    }
    bool update(UINT slot, const BENCHMARK_LIGHT_DATA& data)
    {
        void* mapped = nullptr;
        const D3D12_RANGE read{0, 0};
        if (FAILED(_buffers.at(slot)->Map(0, &read, &mapped))) return false;
        std::memcpy(mapped, &data, sizeof(data));
        _buffers[slot]->Unmap(0, nullptr);
        return true;
    }
    D3D12_GPU_VIRTUAL_ADDRESS address(UINT slot) const { return _buffers.at(slot)->GetGPUVirtualAddress(); }
private:
    std::array<ComPtr<ID3D12Resource>, 2> _buffers;
};
