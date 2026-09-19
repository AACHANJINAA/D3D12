#include "../Common/stdafx.h"
#include "Pipeline.h"
#include "Shader/Shader.h"

bool PIPELINE::initialize(ID3D12Device* device)
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;
    if (!SHADER::get_instance().compile_shader(L"Triangle.hlsl", "VS_Triangle", "vs_5_0", vertex_shader) ||
        !SHADER::get_instance().compile_shader(L"Triangle.hlsl", "PS_Triangle", "ps_5_0", pixel_shader)) return false;

    const CD3DX12_ROOT_PARAMETER root_parameters[] =
    {
        CD3DX12_ROOT_PARAMETER(D3D12_ROOT_PARAMETER_TYPE_CBV, 0, D3D12_SHADER_VISIBILITY_VERTEX)
    };
    const CD3DX12_ROOT_SIGNATURE_DESC root_description(
        _countof(root_parameters), root_parameters,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serialized_root_signature;
    ComPtr<ID3DBlob> root_signature_error;
    if (FAILED(D3D12SerializeRootSignature(&root_description, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root_signature, &root_signature_error)) ||
        FAILED(device->CreateRootSignature(0, serialized_root_signature->GetBufferPointer(),
            serialized_root_signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)))) return false;

    D3D12_INPUT_ELEMENT_DESC input_elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    CD3DX12_GRAPHICS_PIPELINE_STATE_DESC description;
    description.InputLayout = { input_elements, _countof(input_elements) };
    description.pRootSignature = _root_signature.Get();
    description.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    description.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    if (FAILED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_grid_pipeline)))) return false;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    return SUCCEEDED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_cube_pipeline)));
}

ID3D12RootSignature* PIPELINE::get_root_signature() const { return _root_signature.Get(); }
ID3D12PipelineState* PIPELINE::get_grid_pipeline() const { return _grid_pipeline.Get(); }
ID3D12PipelineState* PIPELINE::get_cube_pipeline() const { return _cube_pipeline.Get(); }
