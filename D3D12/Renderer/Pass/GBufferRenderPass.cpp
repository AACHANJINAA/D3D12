#include "../../Common/stdafx.h"
#include "GBufferRenderPass.h"
#include "../../Resource/Texture.h"
#include "../Shader/Shader.h"

bool GBUFFER_RENDER_PASS::initialize(ID3D12Device* device)
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;
    if (!SHADER::get_instance().compile_shader(L"Mesh.hlsl", "VS_Mesh", "vs_5_0", vertex_shader) ||
        !SHADER::get_instance().compile_shader(L"GBuffer.hlsl", "PS_GBuffer", "ps_5_0", pixel_shader))
    {
        return false;
    }

    CD3DX12_ROOT_PARAMETER constant_buffer_parameter(
        D3D12_ROOT_PARAMETER_TYPE_CBV, 0, D3D12_SHADER_VISIBILITY_ALL);
    D3D12_DESCRIPTOR_RANGE texture_range{};
    texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture_range.NumDescriptors = static_cast<UINT>(TEXTURE_SET::texture_count);
    texture_range.BaseShaderRegister = 0;
    texture_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_parameters[2]{};
    root_parameters[0] = constant_buffer_parameter;
    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[1].DescriptorTable.pDescriptorRanges = &texture_range;
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_ROOT_SIGNATURE_DESC root_description{
        2, root_parameters, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    ComPtr<ID3DBlob> serialized_root_signature;
    if (FAILED(D3D12SerializeRootSignature(&root_description, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root_signature, nullptr)) ||
        FAILED(device->CreateRootSignature(0, serialized_root_signature->GetBufferPointer(),
            serialized_root_signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature))))
    {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC input_elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.InputLayout = { input_elements, _countof(input_elements) };
    description.pRootSignature = _root_signature.Get();
    description.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    description.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
    description.RasterizerState = CD3DX12_RASTERIZER_DESC();
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC();
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = target_count;
    for (UINT index = 0; index < target_count; ++index)
        description.RTVFormats[index] = formats[index];
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    return SUCCEEDED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_pipeline)));
}

ID3D12RootSignature* GBUFFER_RENDER_PASS::get_root_signature() const { return _root_signature.Get(); }
ID3D12PipelineState* GBUFFER_RENDER_PASS::get_pipeline() const { return _pipeline.Get(); }


bool GBUFFER_RENDER_PASS::resize(ID3D12Device* device, UINT width, UINT height)
{
    release_targets();
    CD3DX12_DESCRIPTOR_HEAP_DESC heap(target_count, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&_rtv_heap))))
        return false;
    _descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto handle = _rtv_heap->GetCPUDescriptorHandleForHeapStart();
    const CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
    for (UINT index = 0; index < target_count; ++index)
    {
        const auto description = CD3DX12_RESOURCE_DESC::texture_2d(
            formats[index], width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        D3D12_CLEAR_VALUE clear{};
        clear.Format = formats[index];
        if (FAILED(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE,
            &description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clear, IID_PPV_ARGS(&_targets[index]))))
            return false;
        device->CreateRenderTargetView(_targets[index].Get(), nullptr, handle);
        handle.ptr += _descriptor_size;
    }
    return true;
}

void GBUFFER_RENDER_PASS::release_targets()
{
    for (auto& target : _targets) target.Reset();
    _rtv_heap.Reset();
}

void GBUFFER_RENDER_PASS::render(ID3D12GraphicsCommandList* list,
    D3D12_CPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_VIRTUAL_ADDRESS constants,
    ID3D12DescriptorHeap* materials, const D3D12_VERTEX_BUFFER_VIEW& vertices,
    const D3D12_INDEX_BUFFER_VIEW& indices, UINT index_count) const
{
    std::array<D3D12_RESOURCE_BARRIER, target_count> barriers{};
    for (UINT index = 0; index < target_count; ++index)
        barriers[index] = CD3DX12_RESOURCE_BARRIER::transition(_targets[index].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    list->ResourceBarrier(target_count, barriers.data());
    auto handle = _rtv_heap->GetCPUDescriptorHandleForHeapStart();
    list->OMSetRenderTargets(target_count, &handle, TRUE, &depth);
    constexpr float clear[4]{};
    for (UINT index = 0; index < target_count; ++index)
    {
        list->ClearRenderTargetView(handle, clear, 0, nullptr);
        handle.ptr += _descriptor_size;
    }
    list->ClearDepthStencilView(depth, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    list->SetPipelineState(_pipeline.Get());
    list->SetGraphicsRootSignature(_root_signature.Get());
    list->SetGraphicsRootConstantBufferView(0, constants);
    list->SetDescriptorHeaps(1, &materials);
    list->SetGraphicsRootDescriptorTable(1, materials->GetGPUDescriptorHandleForHeapStart());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->IASetVertexBuffers(0, 1, &vertices);
    list->IASetIndexBuffer(&indices);
    list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
    list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    for (UINT index = 0; index < target_count; ++index)
        barriers[index] = CD3DX12_RESOURCE_BARRIER::transition(_targets[index].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(target_count, barriers.data());
}
