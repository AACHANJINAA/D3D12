#include "stdafx.h"
#include "LightManager.h"
#include <stdexcept>


#define POINT_LIGHT 1
#define SPOT_LIGHT 2
#define DIRECTIONAL_LIGHT 3

inline UINT CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D call failed");
    }
}

LightManager::LightManager(){}
LightManager::~LightManager()
{
	destroy();
}

void LightManager::initialize(ID3D12Device* device)
{
	UINT bufferSize = CalcConstantBufferByteSize(sizeof(LightsConstantBuffer));

    D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD; // CPU 접근

	D3D12_RESOURCE_DESC buffer_desc = {};

    buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer_desc.Width = bufferSize;
    buffer_desc.Height = 1;
    buffer_desc.DepthOrArraySize = 1;
    buffer_desc.MipLevels = 1;
    buffer_desc.SampleDesc.Count = 1;
    buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(device->CreateCommittedResource(
				&heap_props,
				D3D12_HEAP_FLAG_NONE,
				&buffer_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&_lightsConstantBuffer)));

    // CPU 매핑 후, 종료전까지 해제 x
    D3D12_RANGE read_range = { 0, 0 }; // CPU에서 이 리소스를 읽지 않을 것임
    ThrowIfFailed(_lightsConstantBuffer->Map(0, &read_range, reinterpret_cast<void**>(&_pCbvDataBegin)));


    set_global_ambient({ 0.2f, 0.2f, 0.2f, 1.0f });

    // 2. 주 방향 조명 (태양) 생성
    Light sun;
    sun.m_bEnable = TRUE;
    sun.m_nType = DIRECTIONAL_LIGHT;
    sun.m_cDiffuse = { 0.8f, 0.8f, 0.8f, 1.0f };
	sun.m_cSpecular = { 0.1f, 0.5f, 0.5f, 1.0f };
		sun.m_vDirection = { -0.3f, -0.8f, +0.4f };
    add_light(std::move(sun));

    update();
}


void LightManager::destroy()
{
    if (_lightsConstantBuffer)
    {
        _lightsConstantBuffer->Unmap(0, nullptr);
        _lightsConstantBuffer = nullptr;
        _pCbvDataBegin = nullptr;
    }
}

void LightManager::update()
{
   // 관리 중인 조명 목록(m_lights)을 상수 버퍼 구조체(m_lightsCBData)로 복사
   _lightsCBData.gnLights = static_cast<int>(_lights.size());
   for (int i = 0; i < _lightsCBData.gnLights; ++i)
   {
       _lightsCBData.gLights[i] = _lights[i];
   }

   // 상수 버퍼 구조체의 내용을 GPU 메모리로 복사
    memcpy(_pCbvDataBegin, &_lightsCBData, sizeof(LightsConstantBuffer));
}

void LightManager::bind(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex)
{
    commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, _lightsConstantBuffer->GetGPUVirtualAddress());
}

int LightManager::add_light(Light && light)
{
    if (_lights.size() < MAX_LIGHTS)
    {
        _lights.push_back(std::move(light));
        return static_cast<int>(_lights.size() - 1);
    }
    return -1; // 최대 조명 개수 초과
}

Light* LightManager::get_light(int index)
{
    if (index >= 0 && index < _lights.size())
    {
        return &_lights[index];
    }
    return nullptr;
}

void LightManager::set_global_ambient(const DirectX::XMFLOAT4& ambient)
{
    _lightsCBData.gcGlobalAmbientLight = ambient;
}

XMFLOAT3 LightManager::get_sun_direction() const
{
    // 첫 번째 Directional Light 찾기
    for (const auto& light : _lights)
    {
        if (light.m_nType == DIRECTIONAL_LIGHT && light.m_bEnable)
        {
            return light.m_vDirection;
        }
    }

    // 기본값 (찾지 못하면)
    return XMFLOAT3(0.05f, -0.4f, -0.82f);
}