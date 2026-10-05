#include "Mesh.hlsl"

struct GBUFFER_OUTPUT
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
    float4 emissive : SV_TARGET3;
    float4 world_position : SV_TARGET4;
};

GBUFFER_OUTPUT PS_GBuffer(
    float4 position : SV_POSITION,
    float3 world_position : POSITION1,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD,
    nointerpolation float instance_selection : TEXCOORD1)
{
    GBUFFER_OUTPUT output;
    float4 albedo = albedo_texture.Sample(texture_sampler, uv) * base_color_factor;
    float4 metal_roughness = metal_roughness_texture.Sample(texture_sampler, uv);
    float metallic_value = saturate(metal_roughness.b * metallic_factor);
    float roughness_value = max(metal_roughness.g * roughness_factor, 0.04f);
    float ambient_occlusion = lerp(1.0f, ao_texture.Sample(texture_sampler, uv).r, occlusion_strength);

    float3 geometric_normal = normalize(normal);
    float3 surface_tangent = normalize(
        tangent.xyz - geometric_normal * dot(geometric_normal, tangent.xyz));
    float3 bitangent = normalize(cross(geometric_normal, surface_tangent) * tangent.w);
    float3 tangent_normal = normal_texture.Sample(texture_sampler, uv).xyz * 2.0f - 1.0f;
    tangent_normal.xy *= normal_scale;
    float3 surface_normal = normalize(
        surface_tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        geometric_normal * tangent_normal.z);

    output.emissive = float4(emissive_texture.Sample(texture_sampler, uv).rgb * emissive_factor, 1.0f);
    output.world_position = float4(world_position, 1.0f);
    output.albedo = albedo;
    output.normal = float4(surface_normal * 0.5f + 0.5f, instance_selection);
    output.material = float4(metallic_value, roughness_value, ambient_occlusion, 1.0f);
    return output;
}

GBUFFER_OUTPUT PS_GBufferCompact(
    float4 position : SV_POSITION, float3 world_position : POSITION1,
    float3 normal : NORMAL, float4 tangent : TANGENT, float2 uv : TEXCOORD,
    nointerpolation float instance_selection : TEXCOORD1)
{
    GBUFFER_OUTPUT output = PS_GBuffer(position, world_position, normal, tangent, uv, instance_selection);
    output.world_position = float4(position.z, 0, 0, 0);
    return output;
}
