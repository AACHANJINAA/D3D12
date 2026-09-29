#pragma once

#include "../../Common/stdafx.h"

class GBUFFER_RENDER_PASS
{
public:
    static constexpr UINT target_count = 5;
    static constexpr DXGI_FORMAT formats[target_count] = {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT
    };
    bool initialize(ID3D12Device* device);
    bool resize(ID3D12Device* device, UINT width, UINT height);
    void release_targets();
    void render(ID3D12GraphicsCommandList* list,
        D3D12_CPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_VIRTUAL_ADDRESS constants,
        ID3D12DescriptorHeap* materials, const D3D12_VERTEX_BUFFER_VIEW& vertices,
        const D3D12_INDEX_BUFFER_VIEW& indices, UINT index_count, bool iswireframe = false) const;
    ID3D12Resource* get_target(UINT index) const { return _targets[index].Get(); }
    ID3D12RootSignature* get_root_signature() const;
    ID3D12PipelineState* get_pipeline() const;

private:
    std::array<ComPtr<ID3D12Resource>, target_count> _targets;
    ComPtr<ID3D12DescriptorHeap> _rtv_heap;
    UINT _descriptor_size = 0;
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _pipeline;
    ComPtr<ID3D12PipelineState> _wireframe_pipeline;
};
