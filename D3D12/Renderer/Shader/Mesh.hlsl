#include "PbrCommon.hlsli"

Texture2D albedo_texture : register(t0);
Texture2D normal_texture : register(t1);
Texture2D metal_roughness_texture : register(t2);
Texture2D ao_texture : register(t3);
Texture2D emissive_texture : register(t4);

struct VERTEX_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VERTEX_OUTPUT
{
    float4 position : SV_POSITION;
    float3 world_position : POSITION1;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VERTEX_OUTPUT VS_Mesh(VERTEX_INPUT input)
{
    VERTEX_OUTPUT output;
    output.world_position = input.position;
    output.position = mul(float4(output.world_position, 1.0f), transform);
    output.normal = input.normal;
    output.tangent = input.tangent;
    output.uv = input.uv;
    return output;
}

float4 PS_Mesh(
    float4 position : SV_POSITION,
    float3 world_position : POSITION1,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD) : SV_TARGET
{
    float4 albedo = albedo_texture.Sample(texture_sampler, uv) * base_color_factor;
    float4 metal_roughness = metal_roughness_texture.Sample(texture_sampler, uv);
    float metallic_value = saturate(metal_roughness.b * metallic_factor);
    float roughness_value = max(metal_roughness.g * roughness_factor, 0.04f);
    float ambient_occlusion = ao_texture.Sample(texture_sampler, uv).r;
    float3 emissive = emissive_texture.Sample(texture_sampler, uv).rgb * emissive_factor;

    float3 geometric_normal = normalize(normal);
    float3 surface_tangent = normalize(
        tangent.xyz - geometric_normal * dot(geometric_normal, tangent.xyz));
    float3 bitangent = normalize(cross(geometric_normal, surface_tangent) * tangent.w);
    float3 tangent_normal = normal_texture.Sample(texture_sampler, uv).xyz * 2.0f - 1.0f;
    float3 surface_normal = normalize(
        surface_tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        geometric_normal * tangent_normal.z);
    return shade_surface(albedo, world_position, surface_normal,
        metallic_value, roughness_value, ambient_occlusion, emissive);
}

float4 PS_Wireframe(float4 position : SV_POSITION) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
