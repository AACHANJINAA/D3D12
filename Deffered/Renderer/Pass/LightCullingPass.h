#pragma once

#include "../../Common/stdafx.h"
#include "GBufferRenderPass.h"

// Deferred light loop variants. Forward always uses the full loop.
enum class LIGHT_CULLING : UINT { none, tiled, clustered };

// Matches cbuffer LIGHT_CULLING (b7) in LightCullingCommon.hlsli.
struct LIGHT_CULLING_CONSTANTS
{
    MATH::MATRIX4X4 view{};
    UINT mode = 0;
    UINT isheatmap = 0;
    UINT grid_x = 1, grid_y = 1;
    UINT slices = 1;
    float z_near = 0.1f, z_far = 100.0f, slice_scale = 1.0f;
    float projection_x = 1.0f, projection_y = 1.0f;
    float viewport_width = 1.0f, viewport_height = 1.0f;
};
static_assert(sizeof(LIGHT_CULLING_CONSTANTS) == 112);

// Builds per-cell light bitmasks before the Deferred lighting pass.
//   tiled     : 16x16 pixel tiles bounded by the G-buffer depth range of each tile
//   clustered : 16x9 screen cells x 24 exponential depth slices, no depth read
class LIGHT_CULLING_PASS
{
public:
    static constexpr UINT tile_size = 16, mask_words = 32;
    static constexpr UINT cluster_x = 16, cluster_y = 9, cluster_z = 24;

    bool initialize(ID3D12Device* device);
    bool set_resources(ID3D12Device* device, const GBUFFER_RENDER_PASS& gbuffer);
    // view_z_min/max bound the visible geometry in view space; used to place cluster slices.
    LIGHT_CULLING_CONSTANTS make_constants(UINT mode, bool isheatmap, const MATH::MATRIX4X4& view,
        const MATH::MATRIX4X4& projection, float view_z_min, float view_z_max) const;
    bool update(UINT slot, const LIGHT_CULLING_CONSTANTS& constants);
    void dispatch(ID3D12GraphicsCommandList* list, UINT slot, D3D12_GPU_VIRTUAL_ADDRESS lights);
    D3D12_GPU_VIRTUAL_ADDRESS address(UINT slot) const { return _constants.at(slot)->GetGPUVirtualAddress(); }
    ID3D12Resource* get_masks() const { return _masks.Get(); }
    UINT get_mask_count() const { return _cell_count * mask_words; }

private:
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _tiled_pipeline, _clustered_pipeline;
    ComPtr<ID3D12DescriptorHeap> _heap;
    ComPtr<ID3D12Resource> _masks;
    std::array<ComPtr<ID3D12Resource>, 2> _constants;
    std::array<LIGHT_CULLING_CONSTANTS, 2> _slot_constants{};
    UINT _width = 0, _height = 0, _cell_count = 0;
};
