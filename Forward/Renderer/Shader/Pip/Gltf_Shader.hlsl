#define MAX_LIGHTS 16

#define POINT_LIGHT 1
#define SPOT_LIGHT 2
#define DIRECTIONAL_LIGHT 3

cbuffer cbMaterial : register(b2)
{
    float4 BaseColorFactor;
    
    float4 EmissiveAndMetallicFactor; // RGB: Emissive, A: Metallic (Offset 16)
#define EmissiveFactor (EmissiveAndMetallicFactor.rgb)
#define MetallicFactor (EmissiveAndMetallicFactor.a)

    float RoughnessFactor; // 다음 16바이트 레지스터 시작 (Offset 32)
    float NormalTextureScale;
    float AlphaCutoff;
    int AlphaMode;

    int DoubleSided; // 다음 16바이트 (Offset 48)
    int HasBaseColorTexture;
    int HasMetallicRoughnessTexture;
    int HasNormalTexture;

    int HasEmissiveTexture; // 다음 16바이트 (Offset 64)
    int HasOcclusionTexture;
    float SpecularFactor;
    float _pad0; // 16바이트 정렬을 위한 패딩

// --- UV Channels ---
    int BaseColorUVChannel;
    int NormalUVChannel;
    int MetallicRoughnessUVChannel;
    int EmissiveUVChannel;

    // --- C++에는 있지만 HLSL에는 빠져있던 UV Transform 변수들 추가 ---
    float2 BaseColorUVOffset;
    float2 BaseColorUVScale;
    float BaseColorUVRotation;
    float _pad1;

    float2 NormalUVOffset;
    float2 NormalUVScale;
    float NormalUVRotation;
    float _pad2;

    float2 MetallicRoughnessUVOffset;
    float2 MetallicRoughnessUVScale;
    float MetallicRoughnessUVRotation;
    float _pad3;
};

Texture2D g_txDiffuse : register(t0);
Texture2D g_txNormal : register(t1);
Texture2D g_txORM : register(t2); // Occlusion, Roughness, Metallic
Texture2D g_txEmissive : register(t3);
Texture2D g_txOcclusion : register(t4);
SamplerState g_samLinear : register(s0);

// 객체별 월드 행렬
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
    int g_bReceiveShadow; // [추가] 0이면 그림자 안 받음, 1이면 받음
    int g_otherplayerid;
    float2 g_worldPad; // 16바이트 정렬을 위한 패딩
};

// 카메라 정보
cbuffer cbCamera : register(b1)
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

 // Light.hlsl에서 사용하는 구조체와 동일하게 맞춤
struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // a = power
    float4 m_cEmissive;
};

 // 임시로 머티리얼 정의 (PBR에서는 사용 안하지만 Light.hlsl 함수에 필요)
static MATERIAL gMaterial =
{
    float4(0.2, 0.2, 0.2, 1.0), // Ambient
	 float4(0.8, 0.8, 0.8, 1.0), // Diffuse
	 float4(1.0, 1.0, 1.0, 32.0), // Specular (a = shininess)
	 float4(0.0, 0.0, 0.0, 1.0) // Emissive
};

#include "Light.hlsl"
#include "IBL.hlsl"
#include "Shadow_Sample.hlsl"

StructuredBuffer<matrix> g_instanceWorldMatrices : register(t12);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD; // 텍스쳐 좌표 (추가된 부분)
    float4 Tangent : TANGENT;
    uint instanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float3 Bitangent : BITANGENT0;
};

VS_OUTPUT VS_GLTF(VS_INPUT input)
{
    VS_OUTPUT Out;

    matrix worldMat = g_instanceWorldMatrices[input.instanceID];

    Out.WorldPosition = mul(float4(input.Position, 1.0f), worldMat).xyz;
    Out.Position = mul(float4(Out.WorldPosition, 1.0f), g_matView);
    Out.Position = mul(Out.Position, g_matProjection);
    Out.TexCoord = input.TexCoord0;

    float3x3 worldMat3x3 = (float3x3) worldMat;

    Out.Normal = normalize(mul(input.Normal, worldMat3x3));
    Out.Tangent = normalize(mul(input.Tangent.xyz, worldMat3x3));
    Out.Tangent = normalize(Out.Tangent - dot(Out.Tangent, Out.Normal) * Out.Normal);
    Out.Bitangent = cross(Out.Normal, Out.Tangent) * input.Tangent.w;

    return Out;
}

float3 ApplyPhantomAura(float3 final_color, float3 N, float3 V)
{
    // 다크 판타지 소울류 팬텀(Phantom) 느낌의 색상 오라
    const float3 phantomColors[4] =
    {
        float3(0.8f, 0.1f, 0.05f),  // [0] 적령 (Blood Red)
        float3(0.2f, 0.5f, 0.9f),   // [1] 청령 (Moonlight Blue)
        float3(0.9f, 0.6f, 0.1f),   // [2] 태양령 (Sunlight Gold)
        float3(0.5f, 0.1f, 0.8f),   // [3] 암령/광령 (Abyssal Purple)
    };

    int colorIndex = g_otherplayerid % 4;
    float3 auraColor = phantomColors[colorIndex];

    // 1. 베이스 틴트 (원래 색상의 명암을 보존하면서 아주 살짝만 물들임)
    float3 tintedColor = final_color * lerp(float3(1, 1, 1), auraColor * 2.0f, 0.15f);

    // 2. 림 라이트 (Fresnel) 오라 효과 (외곽선을 따라 은은하게 빛남)
    float fresnel = pow(1.0 - saturate(dot(N, V)), 2.5);
    float3 glow = auraColor * fresnel * 1.5f;

    return tintedColor + glow;
}

float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 1. Albedo (BaseColor) 값 설정
    float4 diffuseSample = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    float3 albedo = diffuseSample.rgb * BaseColorFactor.rgb;

    // 2. ORM (Occlusion, Roughness, Metallic) 값 설정
    float3 orm = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;
	
    float hasOccTex = step(0.5f, (float) HasOcclusionTexture);
    float hasMetTex = step(0.5f, (float) HasMetallicRoughnessTexture);
	// AO 계산 (기본 1.0 -> ORM 텍스처 -> Occlusion 텍스처 순으로 덮어씌움)
    float ao = lerp(1.0f, orm.r, hasMetTex);
    ao = lerp(ao, g_txOcclusion.Sample(g_samLinear, In.TexCoord).r, hasOccTex);
    
    float roughness = lerp(RoughnessFactor, orm.g * RoughnessFactor, hasMetTex);
    float metallic = lerp(MetallicFactor, orm.b * MetallicFactor, hasMetTex);

     // 3. Normal Map
    float3 N = normalize(In.Normal);
    float3 N_geom = normalize(In.Normal); // 기하학적 Normal 저장

    float3 normalMapSample = g_txNormal.Sample(g_samLinear, In.TexCoord).rgb;

    float3 N_map = normalMapSample * 2.0 - 1.0;
    float3 T = normalize(In.Tangent);
    float3 B = normalize(In.Bitangent);
    float3x3 TBN = float3x3(T, B, N_geom);
    float3 N_mapped = normalize(mul(N_map, TBN));

	// 노멀맵 데이터가 유효한지(길이가 0.1 초과인지) 판별
    float hasNormal = step(0.1f, length(normalMapSample));

	// 조건에 따라 원래 지오메트리 노멀과 맵이 적용된 노멀 사이를 보간
    N = lerp(N_geom, N_mapped, hasNormal);

    // 4. Emissive
    float3 emissiveSample = g_txEmissive.Sample(g_samLinear, In.TexCoord).rgb;
    float3 finalEmissive = emissiveSample * EmissiveFactor;

    // 5. View vector
    float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);

	// faceforward 함수는 세 번째 인자(Normal)를 기준으로 첫 번째 인자(Normal)의 방향을 결정.
	// 두 번째 인자(-V)는 시점에서 표면으로 향하는 벡터의 반대 방향(즉, 표면에서 시점으로 향하는 벡터)입니다.
	// 따라서, 만약 Normal이 시점에서 표면으로 향하는 벡터와 같은 방향을 가리키고 있다면, faceforward는 Normal의 방향을 뒤집어서 시점에서 표면으로 향하도록 합니다.
    N = faceforward(N, -V, N);

    // 1. 직접광 계산 (Light.hlsl의 Lighting 함수)
    float4 litColor = Lighting(In.WorldPosition, N, V, albedo, metallic, roughness, ao, SpecularFactor);

   // 2. 환경광 계산 (IBL.hlsl의 CalculateIBL 함수)
    float3 iblColor = CalculateIBL(N, V, albedo, metallic, roughness, ao);
    
    // View 공간에서의 깊이(Z) 값 계산 (어떤 Cascade를 쓸지 결정하기 위함)
    float3 viewPos = mul(float4(In.WorldPosition, 1.0f), g_matView).xyz;
    float viewDepth = viewPos.z;

    // 그림자 값 샘플링 (0.0: 완전 그림자 ~ 1.0: 빛 받음)
    float shadowFactor = sample_csm_shadow(In.WorldPosition, N, viewDepth);
    
    // 3. 최종 색상 계산: 직접광 + IBL + Emissive, 모두 그림자 영향을 받음
    float3 finalColor = (litColor.rgb * shadowFactor) + iblColor + finalEmissive;

    // 4. 플레이어일 경우 색상 보정 (g_otherplayerid에 따라 색상 변경)
    if (g_otherplayerid > -1) 
        finalColor = ApplyPhantomAura(finalColor, N, V);
    
    // MASK 모드: alphaCutoff 이하의 픽셀을 폐기 (clip 함수 사용)
    if (AlphaMode == 1)
        clip(diffuseSample.a - AlphaCutoff); // 알파가 cutoff보다 작으면 픽셀 폐기
    
    // 톤 매핑 및 감마 보정
    finalColor = finalColor / (finalColor + 1.0f);
    finalColor = pow(finalColor, 1.0f / 2.2f);
    
    return float4(finalColor, diffuseSample.a);
}