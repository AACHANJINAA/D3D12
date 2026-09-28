#include "../Common/stdafx.h"
#include "Pipeline.h"
#include "Shader/Shader.h"

bool PIPELINE::initialize(ID3D12Device* device)
{
    return _cube_pso.initialize(device);
}

ID3D12RootSignature* PIPELINE::get_root_signature() const
{
    return _cube_pso.get_root_signature();
}

ID3D12PipelineState* PIPELINE::get_pipeline() const
{
    return _cube_pso.get_pipeline();
}

ID3D12PipelineState* PIPELINE::get_wireframe_pipeline() const
{
    return _cube_pso.get_wireframe_pipeline();
}


bool CUBE_PSO::initialize(ID3D12Device* device)
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> white_pixel_shader;
    ComPtr<ID3DBlob> black_pixel_shader;
    if (!SHADER::get_instance().compile_shader(L"Triangle.hlsl", "VS_Triangle", "vs_5_0", vertex_shader) ||
        !SHADER::get_instance().compile_shader(L"Triangle.hlsl", "PS_Color", "ps_5_0", white_pixel_shader) ||
        !SHADER::get_instance().compile_shader(L"Triangle.hlsl", "PS_Black", "ps_5_0", black_pixel_shader))
    {
        return false;
    }

    CD3DX12_ROOT_PARAMETER constant_buffer_parameter(
        D3D12_ROOT_PARAMETER_TYPE_CBV, 0, D3D12_SHADER_VISIBILITY_ALL);
    D3D12_DESCRIPTOR_RANGE texture_range{};
    texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture_range.NumDescriptors = 1;
    texture_range.BaseShaderRegister = 0;
    texture_range.RegisterSpace = 0;
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
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_ROOT_SIGNATURE_DESC root_description{
        2, root_parameters, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    ComPtr<ID3DBlob> serialized_root_signature;
    ComPtr<ID3DBlob> root_signature_error;
    if (FAILED(D3D12SerializeRootSignature(&root_description, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root_signature, &root_signature_error)) ||
        FAILED(device->CreateRootSignature(0, serialized_root_signature->GetBufferPointer(),
            serialized_root_signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature))))
    {
        if (root_signature_error != nullptr)
        {
            OutputDebugStringA(static_cast<const char*>(root_signature_error->GetBufferPointer()));
        }
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC input_elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.InputLayout = { input_elements, _countof(input_elements) };
    description.pRootSignature = _root_signature.Get();
    description.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    description.PS = { white_pixel_shader->GetBufferPointer(), white_pixel_shader->GetBufferSize() };
    description.RasterizerState = CD3DX12_RASTERIZER_DESC();
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC();
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;

    if (FAILED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_pipeline))))
    {
        return false;
    }

    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.PS = { black_pixel_shader->GetBufferPointer(), black_pixel_shader->GetBufferSize() };
    return SUCCEEDED(device->CreateGraphicsPipelineState(
        &description, IID_PPV_ARGS(&_wireframe_pipeline)));
}

ID3D12RootSignature* CUBE_PSO::get_root_signature() const { return _root_signature.Get(); }
ID3D12PipelineState* CUBE_PSO::get_pipeline() const { return _pipeline.Get(); }
ID3D12PipelineState* CUBE_PSO::get_wireframe_pipeline() const { return _wireframe_pipeline.Get(); }
