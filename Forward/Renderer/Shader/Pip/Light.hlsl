// 언리얼 스타일 PBR 조명 계산

#define MAX_LIGHTS            16
#define MAX_MATERIALS        512

#define POINT_LIGHT            1
#define SPOT_LIGHT             2
#define DIRECTIONAL_LIGHT     3

#define _WITH_LOCAL_VIEWER_HIGHLIGHTING
#define _WITH_THETA_PHI_CONES
//#define _WITH_REFLECT

struct LIGHT
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular;
    float3 m_vPosition;
    float m_fFalloff;
    float3 m_vDirection;
    float m_fTheta; //cos(m_fTheta)
    float3 m_vAttenuation;
    float m_fPhi; //cos(m_fPhi)
    bool m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

cbuffer cbLights : register(b3)
{
    LIGHT gLights[MAX_LIGHTS];
    float4 gcGlobalAmbientLight;
    float4 g_IblDiffuseSH[9];
    int gnLights;
};

static const float PI = 3.14159265359;

 // Pow5 헬퍼 (Fresnel에서 사용)
float Pow5(float x)
{
    float x2 = x * x;
    return x2 * x2 * x;
}

// F0 계산

// 유전체(비금속)의 Specular 값을 F0로 변환
// Specular 기본값 0.5 = F0 0.04 (4% 반사)
float DielectricSpecularToF0(float Specular)
{
    return 0.04 * Specular;
}

 // 최종 F0 계산 (Metallic에 따라 lerp)
float3 ComputeF0(float Specular, float3 BaseColor, float Metallic)
{
    return lerp(DielectricSpecularToF0(Specular).xxx, BaseColor, Metallic.xxx);
}

// D: Normal Distribution Function (언리얼 최적화 버전)
float D_GGX(float a2, float NoH)
{
    float d = (NoH * a2 - NoH) * NoH + 1;
    float denom = PI * d * d;
    
    denom = max(denom, 0.0001);

    return a2 / denom;
}

 // Vis: Visibility Function (Geometry + 1/(4*NdotV*NdotL) 통합)
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
    float a = sqrt(a2);
    float Vis_SmithV = NoL * (NoV * (1 - a) + a);
    float Vis_SmithL = NoV * (NoL * (1 - a) + a);

    float denom = Vis_SmithV + Vis_SmithL;
    
    denom = max(denom, 0.0001);

    return 0.5 / denom;
}

// F: Fresnel (언리얼 SpecularColor 방식)
float3 F_Schlick(float3 SpecularColor, float VoH)
{
    float Fc = Pow5(1 - VoH); // 1 sub, 3 mul

       // 2% 이하는 물리적으로 불가능하므로 섀도우로 간주
    return saturate(50.0 * SpecularColor.g) * Fc + (1 - Fc) * SpecularColor;
}

// 직접광 계산
float4 Lighting(float3 worldPos, float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao, float specular)
{
       // SpecularColor 계산
    float3 SpecularColor = ComputeF0(specular, albedo, metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);

	[unroll(MAX_LIGHTS)]
    for (int i = 0; i < gnLights; i++)
    {
         // 1. 활성화 및 거리 체크 (continue 대신 가중치 0으로 렌더링 무효화)
        float isEnable = (float) gLights[i].m_bEnable;
        float distance_to_Light = length(gLights[i].m_vPosition - worldPos);

		// Directional은 거리 무관(1.0), 나머지는 거리 체크
        float isDirLight_chk = step(2.5f, (float) gLights[i].m_nType); // DIRECTIONAL(3)
        float inRange = step(distance_to_Light, gLights[i].m_fRange * 1.2f);
        float validLight = isEnable * saturate(isDirLight_chk + inRange);

		// 2. 빛 타입 식별
        float isDir = step(2.5f, (float) gLights[i].m_nType); // 3이면 1.0
        float isSpot = step(1.5f, (float) gLights[i].m_nType) - isDir; // 2이면 1.0
        float isPoint = step(0.5f, (float) gLights[i].m_nType) - step(1.5f, (float) gLights[i].m_nType); // 1이면 1.0

    	// --- DIRECTIONAL 연산 ---
        float3 L_dir = normalize(-gLights[i].m_vDirection);
        float atten_dir = 1.0f;

		// --- POINT 연산 ---
        float3 L_point = normalize(gLights[i].m_vPosition - worldPos);
        float atten_point = 1.0f / dot(gLights[i].m_vAttenuation, float3(1.0f, distance_to_Light, distance_to_Light * distance_to_Light));
        atten_point *= step(distance_to_Light, gLights[i].m_fRange); // 사거리 밖은 0

		// --- SPOT 연산 ---
        float3 L_spot = normalize(gLights[i].m_vPosition - worldPos);
        float fAlpha = max(dot(-L_spot, gLights[i].m_vDirection), 0.0f);
        float fSpotFactor = pow(max(((fAlpha - gLights[i].m_fPhi) / (gLights[i].m_fTheta - gLights[i].m_fPhi)), 0.0f), gLights[i].m_fFalloff);
        float atten_spot = fSpotFactor / dot(gLights[i].m_vAttenuation, float3(1.0f, distance_to_Light, distance_to_Light * distance_to_Light));
        atten_spot *= step(distance_to_Light, gLights[i].m_fRange);

		// 3. 최종 L과 Attenuation 도출 (if문 대체)
        float3 L = L_dir * isDir + L_point * isPoint + L_spot * isSpot;
        L = normalize(L); // 0벡터 방지
        float attenuation = (atten_dir * isDir + atten_point * isPoint + atten_spot * isSpot) * validLight;

    	// 벡터 및 Dot products 계산
        float3 H = normalize(V + L);
        float3 radiance = gLights[i].m_cDiffuse.rgb * attenuation;

        float NoL = saturate(dot(N, L));
        float NoV = saturate(dot(N, V));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

           // Roughness 클램핑
        roughness = max(roughness, 0.15);
        float a = roughness * roughness;

           // ===== 언리얼 BRDF (안전장치 포함) =====
        float D = D_GGX(a, NoH);
        float Vis = Vis_SmithJointApprox(a, NoV, NoL);
        float3 F = F_Schlick(SpecularColor, VoH);

           // Diffuse
        float3 kD = (1.0 - F) * (1.0 - metallic);
        float3 diffuse = kD * albedo;// / PI;

           // Specular
        float3 spec = D * Vis * F;

           // 최종 누적
        Lo += (diffuse + spec) * radiance * NoL;
    }

       // Global Ambient
    float3 ambient = gcGlobalAmbientLight.rgb * albedo * ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}