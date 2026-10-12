#include "stdafx.h"
#include "RootSignature.h"

const std::string& DefaultRootSignatureGenerator::name() const
{
	static const std::string defaultName = "default";
	return defaultName;
}

ComPtr<ID3D12RootSignature> DefaultRootSignatureGenerator::create(ID3D12Device* device)
{
	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 4개의 텍스처(t0, t1, t2, t3)를 포함하는 하나의 Descriptor Range를 정의합니다.
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t0부터 시작하는 4개의 SRV

    CD3DX12_ROOT_PARAMETER params[5];

    // 0번 파라미터: 월드 행렬용 CBV
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // 1번 파라미터: 카메라용 CBV
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
     // 파라미터 4: 텍스처를 위한 하나의 Descriptor Table
	params[4].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    // 정적 샘플러 설정 (기존과 동일)
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 16;
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    d3dStaticSamplerDesc.ShaderRegister = 0; // s0 레지스터
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;
    
    // 루트 시그니처 생성 (기존과 동일)
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));
    
    return pd3dGraphicsRootSignature;
}

const std::string& GltfRootSignatureGenerator::name() const
{
    static const std::string gltfName = "gltf";
	return gltfName;
}

ComPtr<ID3D12RootSignature> GltfRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    //// 1. 텍스처(SRV)를 위한 디스크립터 테이블 설정
    CD3DX12_DESCRIPTOR_RANGE ranges[8]; // 6 -> 8로 확장 (그림자용 SRV)

    for (int i = 0; i < 4; ++i){
        ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t0~t3
    }
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 8, 0,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);     // t8~t10 (IBL)
    ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);     // t4 (Occlusion)

    ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t11: shadow map
    ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 16, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t16: static shadow map

	CD3DX12_ROOT_PARAMETER params[13]; // CBV 4개 + PBR 텍스처 테이블 4개 + IBL 텍스처 테이블 1개 + Occlusion 텍스처 테이블 1개 + shadow 월드 행렬 CBV + Shadow 텍스처 테이블 1개 + instance data SRV

    // 0번 월드 행렬용 CBV
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // 1번 카메라용 CBV
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // 2번 재질용 CBV
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
	// 3번 조명용 CBV
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3

    // 4~7번 PBR 텍스처 디스크립터 테이블 (t0~t3)
    for (int i = 0; i < 4; ++i)
    {
		params[4 + i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3
    }

    // 8번 IBL 텍스처 디스크립터 테이블 (t8~t10, 하나로 통합) 
    params[8].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
    params[9].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL); // t4 Occlusion

	// b5 : shadow CBV
	params[10].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_ALL); // b5: shadow world matrix
	// t11, t16 : shadow descriptor table -> srv
	params[11].InitAsDescriptorTable(2, &ranges[6], D3D12_SHADER_VISIBILITY_PIXEL); // t11, t16: shadow maps
	params[12].InitAsShaderResourceView(12, 0, D3D12_SHADER_VISIBILITY_VERTEX); // t12 : instance data (vertex shader용 SRV)

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    //// 3. 텍스처 샘플러 설정
	// Sampler 확장 (1개 -> 2개) -> 그림자용 샘플러 추가
    D3D12_STATIC_SAMPLER_DESC samplers[2];

    samplers[0].Filter = D3D12_FILTER_ANISOTROPIC;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MipLODBias = 0;
    samplers[0].MaxAnisotropy = 16;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].MinLOD = 0;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0; // 셰이더의 s0 레지스터에 연결
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1] = {};
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].ShaderRegister = 1; // s1
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumParameters = 13;
    d3dRootSignatureDesc.NumStaticSamplers = 2;
    d3dRootSignatureDesc.pStaticSamplers = samplers;

    // 4. 루트 서명 생성
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}

const std::string& SkinnedRootSignatureGenerator::name() const
{
    static const std::string skinnedName = "skinned";
    return skinnedName;
}

ComPtr<ID3D12RootSignature> SkinnedRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 1. 텍스처(SRV)를 위한 디스크립터 범위 지정 (t0 ~ t3)
    CD3DX12_DESCRIPTOR_RANGE ranges[8];
    for (int i = 0; i < 4; ++i)
    {
        ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    }

    // IBL 텍스처 추가 (t8~t10)
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 8, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
	// Shadow map, SRV 추가 (t11, t16)
    ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 16, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

    CD3DX12_ROOT_PARAMETER params[13];

    // [0] b0: 월드 행렬
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // [1] b1: 카메라
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // [2] b2: 재질
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // [3] b3: 조명
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
    // [4~7] t0~t3: 텍스처 디스크립터 테이블 (GltfRootSignature와 위치 동일하게 유지)
    for (int i = 0; i < 4; ++i)
    {
        // 파라미터 인덱스 4, 5, 6, 7
        int param_index = 4 + i;
		params[param_index].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3
    }
    // [8] t8~t10: IBL 텍스처 디스크립터 테이블 (GltfRootSignature와 동일)
    params[8].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);

    params[9].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL); // t4 Occlusion
    // b5: 그림자 상수 버퍼
    params[10].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_ALL);
    // t11, t16: 그림자 맵 디스크립터 테이블
    params[11].InitAsDescriptorTable(2, &ranges[6], D3D12_SHADER_VISIBILITY_PIXEL);
    // b4: 스키닝 뼈대 행렬 (뒤로 이동)
    params[12].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_VERTEX);     // bone b4

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

	// 3. 정적 샘플러 설정 s0 + s1 (그림자용)

    D3D12_STATIC_SAMPLER_DESC samplers[2];
    samplers[0].Filter = D3D12_FILTER_ANISOTROPIC;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MipLODBias = 0;
    samplers[0].MaxAnisotropy = 16;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].MinLOD = 0;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1 추가
    samplers[1] = {};
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].MipLODBias = 0;
    samplers[1].MaxAnisotropy = 0;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[1].MinLOD = 0;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1; // s1
    samplers[1].RegisterSpace = 0;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumStaticSamplers = 2;
    d3dRootSignatureDesc.pStaticSamplers = samplers;

    // 4. 루트 서명 생성
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////skybox///////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const std::string& SkyBoxRootSignatureGenerator::name() const
{
    static const std::string skyboxName = "skybox";
    return skyboxName;
}

ComPtr<ID3D12RootSignature> SkyBoxRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);


    CD3DX12_ROOT_PARAMETER params[5];

    // [0] b0: 월드 행렬 (CBV)
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // [1] b1: 카메라 (CBV)
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b
    // [2] b2: 재질 (CBV) - 스카이박스는 안 써도 칸은 비워둡니다 (호환성)
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // [3] b3: 조명 (CBV) - 마찬가지로 칸 유지
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
    // [4] t0: 텍스처 테이블 (Descriptor Table)
	params[4].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL); // t0

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};

    d3dStaticSamplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;

    // [주소 모드] 스카이박스는 끝부분 선(Seam) 방지를 위해 CLAMP가 안전함
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 16; // LINEAR 쓸 땐 1, ANISOTROPIC 쓸 땐 16
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

    // [레지스터] 셰이더의 register(s0)와 연결
    d3dStaticSamplerDesc.ShaderRegister = 0;
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;

    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;

    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);

    if (pd3dErrorBlob)
    {
        OutputDebugStringA((char*)pd3dErrorBlob->GetBufferPointer());
    }

    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////Terrain//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string& TerrainRootSignatureGenerator::name() const
{
    static const std::string terrainName = "terrain";
    return terrainName;
}

ComPtr<ID3D12RootSignature> TerrainRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Descriptor Range for Textures
    CD3DX12_DESCRIPTOR_RANGE ranges[8]; // 6 -> 8로 확장 (그림자용 SRV)
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t0~t4
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 8, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t9~t10(IBL)
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t11 (shadow) 
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 16, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t16 (static shadow) 
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 12, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t12~t15 (terrain layers)

    // Root Parameters
    CD3DX12_ROOT_PARAMETER params[10];
    // [0] b0: World Matrix (cbPerObject)
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); 
    // [1] b1: Camera (cbPerFrame)
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    // [2] b2: Terrain Info (cbTerrain)
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); 
    // [3] b3: Light (cbPerLight)
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); 
	// [4] t0~t4: Texture Descriptor Table
    params[4].InitAsDescriptorTable(1, &ranges[0]);
    // [5] t8~t10: IBL Texture Descriptor Table 
    params[5].InitAsDescriptorTable(1, &ranges[1]);
	// [6] b5: Shadow Constants
    params[6].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_ALL); 
	// [7] t11, t16: Shadow Texture Descriptor Table
    params[7].InitAsDescriptorTable(2, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
	// [8] b6: Terrain Light Constants
	params[8].InitAsConstantBufferView(6, 0, D3D12_SHADER_VISIBILITY_ALL);
	// [9] t12~t15: Terrain Layer Textures Descriptor Table
    params[9].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    D3D12_STATIC_SAMPLER_DESC samplers[2];
    
    samplers[0].Filter = D3D12_FILTER_ANISOTROPIC;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MipLODBias = 0;
    samplers[0].MaxAnisotropy = 16;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].MinLOD = 0;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1] = {};
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].MipLODBias = 0;
    samplers[1].MaxAnisotropy = 0;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[1].MinLOD = 0;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1; // s1
    samplers[1].RegisterSpace = 0;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Sampler 설정 (Gltf와 동일하게 s1 추가)
    d3dRootSignatureDesc.NumParameters = 10;
    d3dRootSignatureDesc.NumStaticSamplers = 2;
    d3dRootSignatureDesc.pStaticSamplers = samplers; 

    // Create Root Signature
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}


const std::string& UIRootSignatureGenerator::name() const
{
    static const std::string terrainName = "ui";
    return terrainName;
}

ComPtr<ID3D12RootSignature> UIRootSignatureGenerator::create(ID3D12Device* device)
{
    // 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[3];

    // 0: 화면 정보 상수 버퍼 (b0)
    slot_root_parameter[0].InitAsConstantBufferView(0);

    // 1: UI 요소 정보 상수 버퍼 (b1)
    slot_root_parameter[1].InitAsConstantBufferView(1);

    // 2: 텍스처 (t0)
    CD3DX12_DESCRIPTOR_RANGE1 texture_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slot_root_parameter[2].InitAsDescriptorTable(1, &texture_range);

    // 샘플러 설정
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // 필터
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // U 주소 모드
        D3D12_TEXTURE_ADDRESS_MODE_WRAP   // V 주소 모드
    );

    // 루트 시그니처 생성
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        3,                      // 파라미터 개수
        slot_root_parameter,    // 파라미터 배열
        1,                      // 샘플러 개수
        &sampler,               // 샘플러 배열
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (error_blob)
    {
        CERROR("UI Root Signature Serialization Error: "
            << (char*)error_blob->GetBufferPointer());
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}

const std::string& MonsterHPUIRootSignatureGenerator::name() const
{
    static const std::string monster_HP_UI_name = "Monster_HP_UI";
    return monster_HP_UI_name;
}

ComPtr<ID3D12RootSignature> MonsterHPUIRootSignatureGenerator::create(ID3D12Device* device)
{
    // 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[4];

    // [0] b2: HP 바 크기 정보 (g_Size)
    slot_root_parameter[0].InitAsConstantBufferView(2);

    // [1] b1: 카메라 정보 (대원님 요청사항: 반드시 b1)
    slot_root_parameter[1].InitAsConstantBufferView(1);

    // [수정] 2개의 테이블로 분리 (각각 t0, t1)
    static CD3DX12_DESCRIPTOR_RANGE1 range0(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
    static CD3DX12_DESCRIPTOR_RANGE1 range1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

    slot_root_parameter[2].InitAsDescriptorTable(1, &range0); // 슬롯 2 -> t0
    slot_root_parameter[3].InitAsDescriptorTable(1, &range1); // 슬롯 3 -> t1

    // 3. 정적 샘플러 설정 (s0)
    CD3DX12_STATIC_SAMPLER_DESC linear_sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    );

    // 4. 루트 시그니처 설명자 초기화
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        _countof(slot_root_parameter),
        slot_root_parameter,
        1,
        &linear_sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    // 5. 직렬화 및 생성
    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (FAILED(hr))
    {
        if (error_blob)
        {
            CERROR("Monster HP UI Root Signature Error: " << (char*)error_blob->GetBufferPointer());
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}

ComPtr<ID3D12RootSignature> DebugRootSignatureGenerator::create(ID3D12Device* device) {
    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0 (World)
    params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1 (Camera)

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3DBlob> blob, error;
    D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
    return rootSig;
}

const std::string& CsmDepthRootSignatureGenerator::name() const
{
    static const std::string n = "csm_depth";
    return n;
}

ComPtr<ID3D12RootSignature> CsmDepthRootSignatureGenerator::create(ID3D12Device*
    device)
{
    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);// b0
    params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);   // b1 cascades(GS에서 사용)
    params[2].InitAsShaderResourceView(12, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3DBlob> blob, error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)))
    {
        if (error) { OutputDebugStringA((char*)error->GetBufferPointer()); }
    }
    device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));

    return rootSig;
}

const std::string& CsmDepthSkinnedRootSignatureGenerator::name() const
{
    static const std::string n = "csm_depth_skinned";
    return n;
}

ComPtr<ID3D12RootSignature>
CsmDepthSkinnedRootSignatureGenerator::create(ID3D12Device* device)
{
    // param[0] = b0 : 월드 행렬
    // param[1] = b1 : cascade LightVP 3개
    // param[2] = b4 : 뼈대 변환 행렬 128개
    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // b0 world
    params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);    // b1 cascades(VS + GS)
    params[2].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_VERTEX); // b4 bones

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3DBlob> blob, error;
    D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));

    return rootSig;
}

const std::string& UIFrameRootSignatureGenerator::name() const
{
    static const std::string terrainName = "ui_frame";
    return terrainName;
}

ComPtr<ID3D12RootSignature> UIFrameRootSignatureGenerator::create(ID3D12Device* device)
{
    // 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[3];

    // 0: 화면 정보 상수 버퍼 (b0)
    slot_root_parameter[0].InitAsConstantBufferView(0);

    // 1: UI 요소 정보 상수 버퍼 (b1)
    slot_root_parameter[1].InitAsConstantBufferView(1);

    // 2: 텍스처 (t0)
    CD3DX12_DESCRIPTOR_RANGE1 texture_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slot_root_parameter[2].InitAsDescriptorTable(1, &texture_range);

    // 샘플러 설정
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // 필터
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // U 주소 모드
        D3D12_TEXTURE_ADDRESS_MODE_WRAP   // V 주소 모드
    );

    // 루트 시그니처 생성
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        3,                      // 파라미터 개수
        slot_root_parameter,    // 파라미터 배열
        1,                      // 샘플러 개수
        &sampler,               // 샘플러 배열
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (error_blob)
    {
        CERROR("UI Root Signature Serialization Error: "
            << (char*)error_blob->GetBufferPointer());
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}

const std::string& MinimapRootSignatureGenerator::name() const
{
    static const std::string name = "minimap";
    return name;
}

ComPtr<ID3D12RootSignature> MinimapRootSignatureGenerator::create(ID3D12Device* device)
{
    // 1. 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[2];

    // [0] b0: MinimapConstants (CBV)
    slot_root_parameter[0].InitAsConstantBufferView(0);

    // [1] t0: Heightmap 텍스처 (Descriptor Table)
    CD3DX12_DESCRIPTOR_RANGE1 heightmap_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slot_root_parameter[1].InitAsDescriptorTable(1, &heightmap_range);

    // 2. 정적 샘플러 설정 (s0 - Linear Wrap)
    CD3DX12_STATIC_SAMPLER_DESC linear_sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // U 주소 모드
        D3D12_TEXTURE_ADDRESS_MODE_WRAP   // V 주소 모드
    );

    // 3. 루트 시그니처 설명자 초기화
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        _countof(slot_root_parameter),
        slot_root_parameter,
        1,
        &linear_sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    // 4. 직렬화 및 생성
    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (FAILED(hr))
    {
        if (error_blob)
        {
            CERROR("Minimap Root Signature Error: " << (char*)error_blob->GetBufferPointer());
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}

const std::string& OcclusionRootSignatureGenerator::name() const
{
    static std::string n = "occlusion_sig"; 
    return n; 
}

ComPtr<ID3D12RootSignature> OcclusionRootSignatureGenerator::create(ID3D12Device* device)
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsConstants(16, 0); // b0: gWorld (Matrix 16개 float)
    slotRootParameter[1].InitAsConstantBufferView(1); // b1: gViewProj

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);

    ComPtr<ID3D12RootSignature> rootSig;
    device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&rootSig));
    return rootSig;
}

const std::string& ComputeParticleRootSignatureGenerator::name() const
{
    static const std::string sigName = "compute_particle";
    return sigName;
}

ComPtr<ID3D12RootSignature> ComputeParticleRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));

    // 컴퓨트 셰이더 전용 플래그
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    CD3DX12_ROOT_PARAMETER params[3];

    // [0] b0: 상수 버퍼 20개 (행렬 16개 + 위치 3개 + 진행도 1개 + 사라지는 연출 진행도 1개)
    params[0].InitAsConstants(21, 0);
    // [1] t0: 타겟 버퍼 (SRV)
    params[1].InitAsShaderResourceView(0);

    // [2] u0: 현재 버퍼 (UAV)
    params[2].InitAsUnorderedAccessView(0);

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;
    d3dRootSignatureDesc.NumStaticSamplers = 0;
    d3dRootSignatureDesc.pStaticSamplers = nullptr;

    ComPtr<ID3D12RootSignature> pRootSignature = nullptr;
    ComPtr<ID3DBlob> pSignatureBlob, pErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);
    device->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature));

    return pRootSignature;
}

const std::string& ParticleRootSignatureGenerator::name() const
{
    static const std::string sigName = "particle_draw";
    return sigName;
}

ComPtr<ID3D12RootSignature> ParticleRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    // 버텍스 버퍼를 안 쓰고 SV_VertexID를 쓸 것이므로 레이아웃 플래그를 허용합니다.
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_DESCRIPTOR_RANGE texRange[1];
    texRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1: 파티클 텍스처 (t0는 버퍼가 씁니다)

    CD3DX12_ROOT_PARAMETER params[5];
    params[0].InitAsConstantBufferView(0); // [0] b0: 기본 ObjectInfo (엔진 호환용)
    params[1].InitAsConstantBufferView(1); // [1] b1: Camera (빌보딩에 필수!)

    // [2] b2: 파티클 정보 (색상 4개 + 크기 1개 + 진행도 1개 + 파티클 사라지는 연출 진행도 1개 = 총 7개의 float)
    params[2].InitAsConstants(7, 2, 0, D3D12_SHADER_VISIBILITY_ALL);

    // [3] t0: 컴퓨트 셰이더가 연산해둔 파티클 위치 버퍼 (가상 주소로 직접 바인딩)
    params[3].InitAsShaderResourceView(0);

    // [4] t1: 파티클 텍스처 (디스크립터 테이블)
    params[4].InitAsDescriptorTable(1, &texRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

    // 샘플러 설정 (텍스처 필터링용)
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;
    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &sampler;

    ComPtr<ID3D12RootSignature> pRootSignature = nullptr;
    ComPtr<ID3DBlob> pSignatureBlob, pErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);
    device->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature));

    return pRootSignature;
}

const std::string& BillboardUIRootSignatureGenerator::name() const
{
    static const std::string name = "billboard_ui";
    return name;
}

ComPtr<ID3D12RootSignature> BillboardUIRootSignatureGenerator::create(ID3D12Device* device)
{
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0: 텍스처

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    // b2: cbMarker (마커 상수 버퍼) -> Root Parameter 0
    rootParameters[0].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);
    // b1: cbCamera (카메라 상수 버퍼) -> Root Parameter 1
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    // t0: 텍스처 테이블 -> Root Parameter 2
    rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(
        0, // 쉐이더 레지스터 번호
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // 필터
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP   // addressW
    );

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
    {
        if (error != nullptr)
        {
            OutputDebugStringA((char*)error->GetBufferPointer());
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    return rootSignature;
}
