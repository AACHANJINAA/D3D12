#include "PbrCommon.hlsli"

Texture2D<float4> gbuffer_albedo : register(t0);
Texture2D<float4> gbuffer_normal : register(t1);
Texture2D<float4> gbuffer_material : register(t2);
Texture2D<float4> gbuffer_emissive : register(t3);
Texture2D<float4> gbuffer_position : register(t4);

float4 VS_Deferred(uint vertex_id : SV_VertexID) : SV_POSITION
{
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

float4 PS_Deferred(float4 position : SV_POSITION) : SV_TARGET
{
    int3 pixel = int3(int2(position.xy), 0);
    float4 world = gbuffer_position.Load(pixel);
    // Empty pixels keep the skybox already drawn into the back buffer.
    clip(world.w - 0.5f);
    float4 albedo = gbuffer_albedo.Load(pixel);
    float3 normal = normalize(gbuffer_normal.Load(pixel).xyz * 2.0f - 1.0f);
    float3 material = gbuffer_material.Load(pixel).xyz;
    float3 emissive = gbuffer_emissive.Load(pixel).rgb;
    if (view_mode == 1) return float4(pow(saturate(albedo.rgb), 1.0f / 2.2f), 1);
    if (view_mode == 2) return float4(normal * 0.5f + 0.5f, 1);
    if (view_mode == 3) return float4(material.xxx, 1);
    if (view_mode == 4) return float4(material.yyy, 1);
    if (view_mode == 5) return float4(material.zzz, 1);
    if (view_mode == 6)
    {
        float3 color = emissive * exp2(exposure);
        return float4(pow(saturate(color / (color + 1)), 1.0f / 2.2f), 1);
    }
    if (view_mode == 7)
    {
        float view_depth = mul(float4(world.xyz, 1), transform).w;
        float depth = saturate(view_depth / max(depth_range, 0.1f));
        return float4(depth.xxx, 1);
    }
    if (view_mode == 8) return float4(0.9f, 0.9f, 0.9f, 1);
    return shade_surface(albedo, world.xyz, normal, material.x,
        max(material.y, 0.04f), material.z, emissive);
}
