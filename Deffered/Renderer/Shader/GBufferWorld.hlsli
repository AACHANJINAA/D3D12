#ifndef GBUFFER_WORLD_INCLUDED
#define GBUFFER_WORLD_INCLUDED

// Requires gbuffer_position, gbuffer_material and the BENCH_LIGHTS constants.
// Full stores world xyz + valid flag; Compact stores depth and rebuilds the position.
float4 read_gbuffer_world(int3 pixel)
{
    float4 world = gbuffer_position.Load(pixel);
    if (bench_compact)
    {
        float2 ndc = (float2(pixel.xy) + 0.5f) / float2(bench_width, bench_height)
            * float2(2, -2) + float2(-1, 1);
        world = mul(float4(ndc, world.x, 1), bench_inverse_view_projection);
        world = float4(world.xyz / world.w, gbuffer_material.Load(pixel).w);
    }
    return world;
}

#endif
