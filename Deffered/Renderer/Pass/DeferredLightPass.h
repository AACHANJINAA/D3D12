#pragma once

#include "../../Common/stdafx.h"
#include "GBufferRenderPass.h"

class DEFERRED_LIGHT_PASS
{
public:
    bool initialize(ID3D12Device* device);
    bool set_resources(ID3D12Device* device, const GBUFFER_RENDER_PASS& gbuffer,
        ID3D12Resource* environment, ID3D12Resource* brdf, ID3D12Resource* specular);
    void render(ID3D12GraphicsCommandList* list,
        D3D12_GPU_VIRTUAL_ADDRESS constants, D3D12_GPU_VIRTUAL_ADDRESS lights) const;

private:
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _pipeline;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
