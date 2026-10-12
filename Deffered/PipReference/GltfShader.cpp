#include "stdafx.h"
#include "GltfShader.h"

#include "ShadowManager.h"
#include "LightManager.h"

const std::string& GltfShader::pso_name() const
{
	static const std::string name = "gltf";
	return name;
}

D3D12_INPUT_LAYOUT_DESC GltfShader::create_input_layout()
{
    static const D3D12_INPUT_ELEMENT_DESC d3d_input_element_descs[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

	return D3D12_INPUT_LAYOUT_DESC{ d3d_input_element_descs, _countof(d3d_input_element_descs) };
}

D3D12_RASTERIZER_DESC GltfShader::create_rasterizer_state()
{
    D3D12_RASTERIZER_DESC d3dRasterizerDesc;
    ::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
    d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    d3dRasterizerDesc.FrontCounterClockwise = FALSE;
    d3dRasterizerDesc.DepthBias = 0;
    d3dRasterizerDesc.DepthBiasClamp = 0.0f;
    d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
    d3dRasterizerDesc.DepthClipEnable = TRUE;
    d3dRasterizerDesc.MultisampleEnable = FALSE;
    d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
    d3dRasterizerDesc.ForcedSampleCount = 0;
    d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return(d3dRasterizerDesc);
}

D3D12_SHADER_BYTECODE GltfShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Gltf_Shader.hlsl", "VS_GLTF", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE GltfShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Gltf_Shader.hlsl", "PS_GLTF", "ps_5_1", shader_blob);
}

void GltfShader::update_per_object(ID3D12GraphicsCommandList* command_list, class Renderer* renderer,GameObject* object)
{
	Shader::update_per_object(command_list, renderer, object);

    //LightManager::instance()->bind(command_list, 3);

    // 그림자 리소스 바인딩 (Gltf 루트 시그니처: Param 10 == b5, Param 11 == t11
    //ShadowManager::instance()->bind_for_lighting(command_list, 10, 11, renderer);
}

std::string GltfShader::required_root_signature() const
{
	return "gltf";
}
