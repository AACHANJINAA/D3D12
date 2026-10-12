#include "Gltf_Shader.hlsl"
SamplerState benchmark_environment_sampler : register(s2);
#define BENCH_SPECULAR g_PrefilteredMap
#define BENCH_BRDF g_BrdfLut
#define BENCH_SAMPLER benchmark_environment_sampler
#include "../Benchmark/MatchedLighting.hlsli"

VS_OUTPUT VS_Matched(VS_INPUT input)
{
    VS_OUTPUT output = VS_GLTF(input);
    float3x3 world = (float3x3)g_instanceWorldMatrices[input.instanceID];
    float3 c0 = cross(world[1], world[2]);
    float3 c1 = cross(world[2], world[0]);
    float3 c2 = cross(world[0], world[1]);
    float orientation = dot(world[0], c0) < 0 ? -1 : 1;
    output.Normal = normalize(mul(input.Normal, float3x3(c0, c1, c2)) * orientation);
    output.Tangent = normalize(mul(input.Tangent.xyz, world));
    output.Bitangent = input.Tangent.w * orientation;
    return output;
}

float4 PS_Matched(VS_OUTPUT input) : SV_TARGET
{
    float4 albedo = g_txDiffuse.Sample(g_samLinear, input.TexCoord) * BaseColorFactor;
    float2 mr = g_txORM.Sample(g_samLinear, input.TexCoord).gb;
    float metallic = saturate(mr.y * MetallicFactor);
    float roughness = max(mr.x * RoughnessFactor, .04);
    float ao = lerp(1.0, g_txOcclusion.Sample(g_samLinear, input.TexCoord).r, _pad0);
    float3 emissive = g_txEmissive.Sample(g_samLinear, input.TexCoord).rgb * EmissiveFactor;
    float3 n = normalize(input.Normal);
    float3 t = normalize(input.Tangent - n * dot(n, input.Tangent));
    float3 b = normalize(cross(n, t) * input.Bitangent.x);
    float3 map = g_txNormal.Sample(g_samLinear, input.TexCoord).xyz * 2 - 1;
    map.xy *= NormalTextureScale;
    n = normalize(t * map.x + b * map.y + n * map.z);
    return bench_shade(albedo, input.WorldPosition, n, metallic, roughness, ao, emissive,
        gvCameraPosition.xyz, gLights[0].m_vDirection, gLights[0].m_cDiffuse.rgb);
}
