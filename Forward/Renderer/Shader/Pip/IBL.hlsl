// IBL.hlsl
    // Image-Based Lighting (환경광 기반 조명) 계산 셰이더
    // PBR 렌더링의 간접광(Indirect Lighting)을 처리합니다.

#ifndef _IBL_HLSL_
#define _IBL_HLSL_

// ============================================================
// IBL 텍스처 선언
// ============================================================

// SH 계수를 이용한 Irradiance 계산 (Ramamoorthi et al. 공식)
float3 CalculateIrradianceSH(float3 N)
{
    const float C1 = 0.429043;
    const float C2 = 0.511664;
    const float C3 = 0.743125;
    const float C4 = 0.886227;
    const float C5 = 0.247708;

    float3 L00 = g_IblDiffuseSH[0].rgb;
    float3 L1_1 = g_IblDiffuseSH[1].rgb;
    float3 L10 = g_IblDiffuseSH[2].rgb;
    float3 L11 = g_IblDiffuseSH[3].rgb;
    float3 L2_2 = g_IblDiffuseSH[4].rgb;
    float3 L2_1 = g_IblDiffuseSH[5].rgb;
    float3 L20 = g_IblDiffuseSH[6].rgb;
    float3 L21 = g_IblDiffuseSH[7].rgb;
    float3 L22 = g_IblDiffuseSH[8].rgb;

    return (
        C4 * L00 -
        C1 * L22 * (N.x * N.x - N.y * N.y) +
        C3 * L20 * N.z * N.z -
        C5 * L20 +
        2.0 * C1 * L2_2 * N.x * N.y +
        2.0 * C1 * L21 * N.x * N.z +
        2.0 * C1 * L2_1 * N.y * N.z +
        2.0 * C2 * L11 * N.x +
        2.0 * C2 * L1_1 * N.y +
        2.0 * C2 * L10 * N.z
    );
}

TextureCube g_IrradianceDummy : register(t8);
// t9: Prefiltered Environment Map - Specular IBL용 (Mipmap으로 Roughness 표현)
TextureCube g_PrefilteredMap : register(t9);
// t10: BRDF LUT - Split-Sum Approximation 용 (NdotV, Roughness -> scale, bias)
Texture2D g_BrdfLut : register(t10);

// 주의: SamplerState g_samLinear는 Gltf_Shader.hlsl에서 선언됨
// IBL.hlsl은 Gltf_Shader.hlsl에 include되므로 자동으로 사용 가능

// Diffuse IBL 계산
float3 CalculateDiffuseIBL(float3 N, float3 albedo, float metallic)
{
	// 1. F0 계산 (표면 반사율)
	float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

	// 2. kD 계산 (Diffuse 강도)
	float3 kD = (1.0 - F0) * (1.0 - metallic);

	// 3. Irradiance Map 샘플링
	//float3 irradiance = g_IrradianceMap.Sample(g_samLinear, N).rgb;
    float3 irradiance = max(CalculateIrradianceSH(N), 0.0);
	
	// 4. Lambertian Diffuse BRDF 계산
	return kD * albedo * irradiance;
}

// Specular IBL 계산
float3 CalculateSpecularIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
	// 1. 반사 벡터 계산
	float3 R = reflect(-V, N);

	// 2. NdotV 계산
	float NdotV = saturate(dot(N, V));

	// 3. Prefiltered Environment Map LOD 선택
	float maxMipLevel = 8.0;
	float safeRoughness = max(roughness, 0.05);
    float lod = safeRoughness * maxMipLevel;

	// 4. Prefiltered Map 샘플링
	float3 prefilteredColor = g_PrefilteredMap.SampleLevel(g_samLinear, R, lod).rgb;

	// 5. BRDF LUT 샘플링
    float2 brdf = g_BrdfLut.SampleLevel(g_samLinear, float2(NdotV, roughness), 0).rg;

	// 6. F0 계산
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

	// 7. Single Scattering Specular 계산
    float3 specularSingle = F0 * brdf.x + brdf.y;

	// 8. Multiple Scattering Energy Compensation
	// Ess = Single Scattering의 총 에너지
	float Ess = brdf.r + brdf.g;
	Ess = max(Ess, 0.1); // 0으로 나누기 방지

	// 9. 최종 Specular = Single Scattering 
    return prefilteredColor * specularSingle;
}

    // IBL 통합 함수
float3 CalculateIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao)
{
	// 1. Diffuse IBL 계산 (주로 비금속 재질의 색감을 담당)
    float3 diffuse = CalculateDiffuseIBL(N, albedo, metallic);

    // 2. Specular IBL 계산
    float3 specular = CalculateSpecularIBL(N, V, albedo, metallic, roughness);

    // 3. Metalic 수치에 따라 최소 스페큘러 강도를 설정
	// metallic / 0.1f는 metallic이 0.1 이상일 때 1.0 이상의 값을 갖게 됩니다.
	// 이를 saturate 함수로 감싸서 값을 [0, 1] 사이로 제한
	// metallic이 0이면 lerp(0.0, 0.4, 0) = 0.4 가 되고, 0.1 이상이면 lerp(0.0, 0.4, 1) = 0.4이 됩니다.
    float specular_scale = lerp(0.0f, 0.4f, saturate(metallic / 0.1f));
    specular *= specular_scale;

    // 4. 합산 후 AO 적용
    return (diffuse + specular) * ao;
}
#endif // _IBL_HLSL_