#include "GBufferWorld.hlsli"

float4 draw_gbuffer_overview(float2 position)
{
    uint width, height;
    gbuffer_albedo.GetDimensions(width, height);
    float2 size = float2(width, height);
    float2 tiled = position / size * 2;
    uint2 tile = min(uint2(tiled), uint2(1, 1));
    int3 pixel = int3(min(int2(frac(tiled) * size), int2(width, height) - 1), 0);
    float4 world = read_gbuffer_world(pixel);
    if (world.w < 0.5f) return float4(0.055f, 0.06f, 0.07f, 1);

    uint index = tile.y * 2 + tile.x;
    if (index == 0)
        return float4(pow(saturate(gbuffer_albedo.Load(pixel).rgb), 1.0f / 2.2f), 1);
    if (index == 1)
        return float4(normalize(gbuffer_normal.Load(pixel).xyz * 2 - 1) * 0.5f + 0.5f, 1);
    if (index == 2) return float4(gbuffer_material.Load(pixel).rgb, 1);
    float depth = saturate(mul(float4(world.xyz, 1), transform).w / max(depth_range, 0.1f));
    return float4(depth.xxx, 1);
}
