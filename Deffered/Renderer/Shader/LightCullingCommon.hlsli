#ifndef LIGHT_CULLING_COMMON_INCLUDED
#define LIGHT_CULLING_COMMON_INCLUDED

// 1024 point lights -> 32 words per cell. Each cell stores a bitmask, not an index list,
// so the light order stays ascending and no per-cell capacity can overflow.
#define LIGHT_MASK_WORDS 32
#define LIGHT_TILE_SIZE 16

// Matches LIGHT_CULLING_CONSTANTS in LightCullingPass.h.
cbuffer LIGHT_CULLING : register(b7)
{
    row_major float4x4 culling_view;
    uint culling_mode;          // 0 = full loop, 1 = tiled, 2 = clustered
    uint culling_heatmap;
    uint2 culling_grid;         // tiles or clusters along x / y
    uint culling_slices;
    float culling_z_near;       // cluster depth range in view space
    float culling_z_far;
    float culling_slice_scale;  // slices / log(far / near)
    float2 culling_projection;  // P[0][0], P[1][1]
    float2 culling_viewport;
};

uint culling_slice(float view_z)
{
    float slice = floor(log(max(view_z, 1e-6f) / culling_z_near) * culling_slice_scale);
    return (uint)clamp(slice, 0.0f, (float)(culling_slices - 1));
}

float culling_slice_depth(uint slice)
{
    return culling_z_near * exp((float)slice / culling_slice_scale);
}

uint culling_cell(float2 pixel, float view_z)
{
    if (culling_mode == 1)
    {
        uint2 tile = uint2(pixel) / LIGHT_TILE_SIZE;
        return tile.y * culling_grid.x + tile.x;
    }
    uint2 cell = min(uint2(pixel / culling_viewport * float2(culling_grid)), culling_grid - 1);
    return (culling_slice(view_z) * culling_grid.y + cell.y) * culling_grid.x + cell.x;
}

#endif
