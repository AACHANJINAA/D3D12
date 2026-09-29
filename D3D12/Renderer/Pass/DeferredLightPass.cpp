#include "../../Common/stdafx.h"
#include "DeferredLightPass.h"
#include "../Shader/Shader.h"

bool DEFERRED_LIGHT_PASS::initialize(ID3D12Device* device)
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;
    if (!SHADER::get_instance().compile_shader(
        L"DeferredLight.hlsl", "VS_Deferred", "vs_5_0", vertex_shader) ||
        !SHADER::get_instance().compile_shader(
            L"DeferredLight.hlsl", "PS_Deferred", "ps_5_0", pixel_shader))
        return false;

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = GBUFFER_RENDER_PASS::target_count + 3;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0] = CD3DX12_ROOT_PARAMETER(
        D3D12_ROOT_PARAMETER_TYPE_CBV, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = { 1, &range };
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    const D3D12_ROOT_SIGNATURE_DESC root{ 2, parameters, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_NONE };
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &error)))
    {
        if (error) OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&_root_signature))))
        return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = _root_signature.Get();
    description.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    description.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
    description.RasterizerState = CD3DX12_RASTERIZER_DESC();
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC();
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
    description.DepthStencilState.DepthEnable = FALSE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    return SUCCEEDED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_pipeline)));
}

bool DEFERRED_LIGHT_PASS::set_resources(ID3D12Device* device,
    const GBUFFER_RENDER_PASS& gbuffer, ID3D12Resource* environment,
    ID3D12Resource* brdf, ID3D12Resource* specular)
{
    if (!environment || !brdf || !specular) return false;
    constexpr UINT count = GBUFFER_RENDER_PASS::target_count + 3;
    CD3DX12_DESCRIPTOR_HEAP_DESC heap(count, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&_srv_heap))))
        return false;
    auto handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ID3D12Resource* resources[count]{};
    for (UINT index = 0; index < GBUFFER_RENDER_PASS::target_count; ++index)
        resources[index] = gbuffer.get_target(index);
    resources[5] = environment;
    resources[6] = brdf;
    resources[7] = specular;
    for (UINT index = 0; index < count; ++index)
    {
        if (!resources[index]) return false;
        const auto description = resources[index]->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = description.Format;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (index == 5 || index == 7)
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view.TextureCube.MipLevels = description.MipLevels;
        }
        else
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipLevels = description.MipLevels;
        }
        device->CreateShaderResourceView(resources[index], &view, handle);
        handle.ptr += stride;
    }
    return true;
}

void DEFERRED_LIGHT_PASS::render(ID3D12GraphicsCommandList* list,
    D3D12_GPU_VIRTUAL_ADDRESS constants) const
{
    list->SetPipelineState(_pipeline.Get());
    list->SetGraphicsRootSignature(_root_signature.Get());
    list->SetGraphicsRootConstantBufferView(0, constants);
    ID3D12DescriptorHeap* heaps[] = { _srv_heap.Get() };
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRootDescriptorTable(1, _srv_heap->GetGPUDescriptorHandleForHeapStart());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
}
