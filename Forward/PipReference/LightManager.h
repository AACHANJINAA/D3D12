#pragma once

#include "stdafx.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

constexpr uint8_t MAX_LIGHTS = 16; // 최대 조명 수 (필요에 따라 조정 가능)

struct Light
{
    XMFLOAT4 m_cAmbient = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT4 m_cDiffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT4 m_cSpecular = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 m_vPosition = { 0.0f, 0.0f, 0.0f };
    float m_fFalloff = 1.0f;
    XMFLOAT3 m_vDirection = { 0.0f, -1.0f, 0.0f };
    float m_fTheta = 0.5f; // cos(theta)
    XMFLOAT3 m_vAttenuation = { 1.0f, 0.0f, 0.0f };
    float m_fPhi = 0.8f;   // cos(phi)
    int m_bEnable = false;
    int m_nType = 0;
    float m_fRange = 1000.0f;
    float padding = 0.0f; // 구조체 크기를 16바이트의 배수로 맞추기 위한 패딩
};

struct LightsConstantBuffer
{
    Light gLights[MAX_LIGHTS];
    XMFLOAT4 gcGlobalAmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT4 g_IblDiffuseSH[9]; // 추가: 9개의 SH 계수 (float4 사용 권장)
    int gnLights = 0;
    XMFLOAT3 padding;
};

class LightManager : public Singleton<LightManager>
{
public:
    friend Singleton<LightManager>;
    // 초기화 및 소멸
    void initialize(ID3D12Device* device);
    void destroy();

    // 매 프레임 호출하여 조명 버퍼의 내용을 GPU로 업데이트
    void update();

    void bind(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex);

    // 조명 추가/조회
    int add_light(Light && light);
    Light* get_light(int index);
    const std::vector<Light>& get_lights() const { return _lights; }

    void set_global_ambient(const DirectX::XMFLOAT4& ambient);
    void set_ibl_diffuse_sh(const DirectX::XMFLOAT4* sh_coefficients) {
        memcpy(_lightsCBData.g_IblDiffuseSH, sh_coefficients, sizeof(XMFLOAT4) * 9);
    }

    XMFLOAT3 get_sun_direction() const;

private:
    LightManager();
    ~LightManager();

    // 복사 및 이동 생성/대입을 막음 (싱글턴)
    LightManager(const LightManager&) = delete;
    LightManager & operator=(const LightManager&) = delete;
    LightManager(LightManager&&) = delete;
    LightManager & operator=(LightManager&&) = delete;

    // 조명 데이터
    std::vector<Light> _lights;
    LightsConstantBuffer _lightsCBData;

    // D3D12 조명 상수 버퍼 리소스
    ComPtr<ID3D12Resource> _lightsConstantBuffer;
    UINT8 * _pCbvDataBegin = nullptr; // GPU 버퍼에 매핑된 CPU 메모리 포인터
};

