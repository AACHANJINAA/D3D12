#define BENCH_LIGHTS_ONLY
#include "Benchmark/MatchedLighting.hlsli"
#include "LightCullingCommon.hlsli"

Texture2D<float4> gbuffer_position : register(t0);
Texture2D<float4> gbuffer_material : register(t1);
RWStructuredBuffer<uint> light_masks : register(u0);

#include "GBufferWorld.hlsli"

// View-space AABB of the frustum piece covering a screen rectangle between two view depths.
// View x = ndc.x * z / P00 is linear in both ndc and z, so the 8 corners bound the piece.
void cell_bounds(float2 pixel_min, float2 pixel_max, float z_min, float z_max,
    out float3 bounds_min, out float3 bounds_max)
{
    float2 ndc_low = float2(pixel_min.x, pixel_max.y) / culling_viewport * float2(2, -2) + float2(-1, 1);
    float2 ndc_high = float2(pixel_max.x, pixel_min.y) / culling_viewport * float2(2, -2) + float2(-1, 1);
    float2 a = ndc_low * z_min / culling_projection, b = ndc_low * z_max / culling_projection;
    float2 c = ndc_high * z_min / culling_projection, d = ndc_high * z_max / culling_projection;
    // Small margin so float rounding in position reconstruction never drops a touching light.
    float margin = max(z_max, 1.0f) * 1e-4f;
    bounds_min = float3(min(min(a, b), min(c, d)), z_min) - margin;
    bounds_max = float3(max(max(a, b), max(c, d)), z_max) + margin;
}

// A light whose sphere misses the cell has falloff == 0 for every pixel inside it.
bool light_overlaps(uint light, float3 bounds_min, float3 bounds_max)
{
    float4 sphere = bench_points[light].position_radius;
    float3 center = mul(float4(sphere.xyz, 1), culling_view).xyz;
    float3 gap = max(max(bounds_min - center, center - bounds_max), 0);
    float radius = sphere.w * 1.0001f;
    return dot(gap, gap) <= radius * radius;
}

groupshared uint tile_min_z;
groupshared uint tile_max_z;
groupshared uint cell_mask[LIGHT_MASK_WORDS];

// Tiled: one group per 16x16 tile. The tile's depth range comes from the G-buffer,
// so a tile mixing near and far surfaces keeps every light in between.
[numthreads(LIGHT_TILE_SIZE, LIGHT_TILE_SIZE, 1)]
void CS_Tiled(uint3 group : SV_GroupID, uint3 thread : SV_DispatchThreadID, uint index : SV_GroupIndex)
{
    if (index == 0)
    {
        tile_min_z = 0x7f7fffff;
        tile_max_z = 0;
    }
    if (index < LIGHT_MASK_WORDS) cell_mask[index] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (all(float2(thread.xy) < culling_viewport))
    {
        float4 world = read_gbuffer_world(int3(thread.xy, 0));
        if (world.w > 0.5f)
        {
            // Positive floats keep their order as uints.
            uint depth = asuint(max(mul(float4(world.xyz, 1), culling_view).z, 0.0f));
            InterlockedMin(tile_min_z, depth);
            InterlockedMax(tile_max_z, depth);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (tile_min_z <= tile_max_z)
    {
        float2 pixel_min = float2(group.xy * LIGHT_TILE_SIZE);
        float2 pixel_max = min(pixel_min + LIGHT_TILE_SIZE, culling_viewport);
        float3 bounds_min, bounds_max;
        cell_bounds(pixel_min, pixel_max, asfloat(tile_min_z), asfloat(tile_max_z), bounds_min, bounds_max);
        for (uint light = index; light < bench_count; light += LIGHT_TILE_SIZE * LIGHT_TILE_SIZE)
            if (light_overlaps(light, bounds_min, bounds_max))
                InterlockedOr(cell_mask[light / 32], 1u << (light % 32));
    }
    GroupMemoryBarrierWithGroupSync();

    if (index < LIGHT_MASK_WORDS)
        light_masks[(group.y * culling_grid.x + group.x) * LIGHT_MASK_WORDS + index] = cell_mask[index];
}

// Clustered: one group per froxel (x, y, exponential depth slice). Independent of the G-buffer,
// so depth discontinuities inside a screen tile do not widen the light list.
[numthreads(64, 1, 1)]
void CS_Clustered(uint3 group : SV_GroupID, uint index : SV_GroupIndex)
{
    if (index < LIGHT_MASK_WORDS) cell_mask[index] = 0;
    GroupMemoryBarrierWithGroupSync();

    float2 cell_size = culling_viewport / float2(culling_grid);
    // Slice 0 also covers anything nearer than the scene range.
    float z_min = group.z == 0 ? 0.0f : culling_slice_depth(group.z);
    float z_max = culling_slice_depth(group.z + 1);
    float3 bounds_min, bounds_max;
    cell_bounds(float2(group.xy) * cell_size, float2(group.xy + 1) * cell_size, z_min, z_max,
        bounds_min, bounds_max);
    for (uint light = index; light < bench_count; light += 64)
        if (light_overlaps(light, bounds_min, bounds_max))
            InterlockedOr(cell_mask[light / 32], 1u << (light % 32));
    GroupMemoryBarrierWithGroupSync();

    if (index < LIGHT_MASK_WORDS)
    {
        uint cell = (group.z * culling_grid.y + group.y) * culling_grid.x + group.x;
        light_masks[cell * LIGHT_MASK_WORDS + index] = cell_mask[index];
    }
}
