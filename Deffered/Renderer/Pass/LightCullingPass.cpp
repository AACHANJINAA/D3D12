#include "../../Common/stdafx.h"
#include "LightCullingPass.h"
#include "../Shader/Shader.h"

bool LIGHT_CULLING_PASS::initialize(ID3D12Device* device)
{
    ComPtr<ID3DBlob> tiled_shader;
    ComPtr<ID3DBlob> clustered_shader;
    if (!SHADER::get_instance().compile_shader(L"LightCulling.hlsl", "CS_Tiled", "cs_5_0", tiled_shader) ||
        !SHADER::get_instance().compile_shader(L"LightCulling.hlsl", "CS_Clustered", "cs_5_0", clustered_shader))
        return false;

    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 2;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0] = CD3DX12_ROOT_PARAMETER(D3D12_ROOT_PARAMETER_TYPE_CBV, 7, D3D12_SHADER_VISIBILITY_ALL);
    parameters[1] = CD3DX12_ROOT_PARAMETER(D3D12_ROOT_PARAMETER_TYPE_CBV, 6, D3D12_SHADER_VISIBILITY_ALL);
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable = { 2, ranges };
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    const D3D12_ROOT_SIGNATURE_DESC root{ 3, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error)))
    {
        if (error) OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&_root_signature))))
        return false;

    D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
    description.pRootSignature = _root_signature.Get();
    description.CS = { tiled_shader->GetBufferPointer(), tiled_shader->GetBufferSize() };
    if (FAILED(device->CreateComputePipelineState(&description, IID_PPV_ARGS(&_tiled_pipeline)))) return false;
    description.CS = { clustered_shader->GetBufferPointer(), clustered_shader->GetBufferSize() };
    if (FAILED(device->CreateComputePipelineState(&description, IID_PPV_ARGS(&_clustered_pipeline)))) return false;

    const CD3DX12_HEAP_PROPERTIES upload(D3D12_HEAP_TYPE_UPLOAD);
    const auto constants = CD3DX12_RESOURCE_DESC::buffer(256);
    for (auto& buffer : _constants)
        if (FAILED(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &constants,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)))) return false;
    for (UINT slot = 0; slot < 2; ++slot) if (!update(slot, {})) return false;

    CD3DX12_DESCRIPTOR_HEAP_DESC heap(3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    return SUCCEEDED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&_heap)));
}

bool LIGHT_CULLING_PASS::set_resources(ID3D12Device* device, const GBUFFER_RENDER_PASS& gbuffer)
{
    ID3D12Resource* position = gbuffer.get_target(4);
    ID3D12Resource* material = gbuffer.get_target(2);
    if (!position || !material || !_heap) return false;
    const auto target = position->GetDesc();
    _width = static_cast<UINT>(target.Width);
    _height = target.Height;
    const UINT tiles = ((_width + tile_size - 1) / tile_size) * ((_height + tile_size - 1) / tile_size);
    const UINT cells = (std::max)(tiles, cluster_x * cluster_y * cluster_z);
    // Only reached after wait_for_gpu (startup, resize, G-buffer layout change).
    if (!_masks || cells > _cell_count)
    {
        _masks.Reset();
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
        auto description = CD3DX12_RESOURCE_DESC::buffer(static_cast<UINT64>(cells) * mask_words * sizeof(UINT));
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        // Buffers start in COMMON and decay back to it after each submission.
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_masks)))) return false;
        _cell_count = cells;
    }

    const UINT stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto handle = _heap->GetCPUDescriptorHandleForHeapStart();
    for (ID3D12Resource* resource : { position, material })
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = resource->GetDesc().Format;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(resource, &view, handle);
        handle.ptr += stride;
    }
    D3D12_UNORDERED_ACCESS_VIEW_DESC access{};
    access.Format = DXGI_FORMAT_UNKNOWN;
    access.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    access.Buffer.NumElements = get_mask_count();
    access.Buffer.StructureByteStride = sizeof(UINT);
    device->CreateUnorderedAccessView(_masks.Get(), nullptr, &access, handle);
    return true;
}

LIGHT_CULLING_CONSTANTS LIGHT_CULLING_PASS::make_constants(UINT mode, bool isheatmap,
    const MATH::MATRIX4X4& view, const MATH::MATRIX4X4& projection, float view_z_min, float view_z_max) const
{
    LIGHT_CULLING_CONSTANTS constants;
    constants.view = view;
    constants.mode = mode;
    constants.isheatmap = isheatmap ? 1 : 0;
    constants.viewport_width = static_cast<float>((std::max)(_width, 1u));
    constants.viewport_height = static_cast<float>((std::max)(_height, 1u));
    constants.projection_x = projection.values[0][0];
    constants.projection_y = projection.values[1][1];
    if (mode == static_cast<UINT>(LIGHT_CULLING::tiled))
    {
        constants.grid_x = (_width + tile_size - 1) / tile_size;
        constants.grid_y = (_height + tile_size - 1) / tile_size;
    }
    else
    {
        constants.grid_x = cluster_x;
        constants.grid_y = cluster_y;
        constants.slices = cluster_z;
    }
    // Left-handed perspective: z' = z * P22 + P32, so near = -P32 / P22, far = P32 / (1 - P22).
    const float near_plane = -projection.values[3][2] / projection.values[2][2];
    const float far_plane = projection.values[3][2] / (1.0f - projection.values[2][2]);
    // Exponential slices over the visible scene only; a 1% margin absorbs reconstruction error.
    float z_near = (std::clamp)(view_z_min * 0.99f, near_plane, far_plane);
    float z_far = (std::clamp)(view_z_max * 1.01f, near_plane, far_plane);
    if (!(z_far > z_near * 1.001f)) { z_near = near_plane; z_far = far_plane; }
    constants.z_near = z_near;
    constants.z_far = z_far;
    constants.slice_scale = static_cast<float>(constants.slices) / std::log(z_far / z_near);
    return constants;
}

bool LIGHT_CULLING_PASS::update(UINT slot, const LIGHT_CULLING_CONSTANTS& constants)
{
    void* mapped = nullptr;
    const D3D12_RANGE read{ 0, 0 };
    if (FAILED(_constants.at(slot)->Map(0, &read, &mapped))) return false;
    std::memcpy(mapped, &constants, sizeof(constants));
    _constants[slot]->Unmap(0, nullptr);
    _slot_constants[slot] = constants;
    return true;
}

void LIGHT_CULLING_PASS::dispatch(ID3D12GraphicsCommandList* list, UINT slot, D3D12_GPU_VIRTUAL_ADDRESS lights)
{
    const auto& constants = _slot_constants.at(slot);
    if (constants.mode == static_cast<UINT>(LIGHT_CULLING::none)) return;
    const bool istiled = constants.mode == static_cast<UINT>(LIGHT_CULLING::tiled);
    list->SetPipelineState(istiled ? _tiled_pipeline.Get() : _clustered_pipeline.Get());
    list->SetComputeRootSignature(_root_signature.Get());
    list->SetComputeRootConstantBufferView(0, address(slot));
    list->SetComputeRootConstantBufferView(1, lights);
    ID3D12DescriptorHeap* heaps[] = { _heap.Get() };
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootDescriptorTable(2, _heap->GetGPUDescriptorHandleForHeapStart());
    // The mask buffer is implicitly promoted from COMMON to UNORDERED_ACCESS here.
    list->Dispatch(constants.grid_x, constants.grid_y, constants.slices);
    const auto to_read = CD3DX12_RESOURCE_BARRIER::transition(_masks.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1, &to_read);
}
