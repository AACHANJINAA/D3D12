#pragma once
#include "../../Scene/Scene.h"
#include "../ViewerSettings.h"

class PIP_FORWARD_PASS
{
public:
    static constexpr UINT frame_count = 2;
    bool initialize(ID3D12Device* device, bool isbackface_culling = true, bool ismatched = false);
    bool update(ID3D12Device* device, UINT frame, const SCENE& scene, const VIEWER_SETTINGS& settings,
        const MATH::MATRIX4X4& view, const MATH::MATRIX4X4& projection,
        const MATH::VECTOR3& camera, ID3D12Resource* environment, ID3D12Resource* specular, ID3D12Resource* brdf,
        bool isinstancing = true, bool iscached = true);
    size_t get_srv_writes(UINT frame) const { return _frames[frame].srv_writes; }
    size_t get_draw_count(UINT frame) const { return _frames[frame].draws.size(); }
    size_t get_instance_count(UINT frame) const
    {
        size_t count = 0;
        for (const auto& draw : _frames[frame].draws) count += draw.instance_count;
        return count;
    }
    void render(ID3D12GraphicsCommandList* list, UINT frame, bool iswireframe,
        D3D12_GPU_VIRTUAL_ADDRESS lights = 0) const;
    void reset();

private:
    struct DRAW
    {
        std::shared_ptr<MODEL_RESOURCE> model;
        UINT primitive = 0, first_instance = 0, instance_count = 0;
    };
    struct FRAME
    {
        ComPtr<ID3D12Resource> buffer;
        UINT8* mapped = nullptr;
        size_t capacity = 0, instance_offset = 0;
        ComPtr<ID3D12DescriptorHeap> heap;
        size_t descriptor_capacity = 0;
        std::vector<DRAW> draws;
        std::vector<std::array<ID3D12Resource*, 10>> descriptor_resources;
        size_t srv_writes = 0;
    };
    std::array<FRAME, frame_count> _frames;
    ComPtr<ID3D12RootSignature> _root;
    ComPtr<ID3D12PipelineState> _pipeline, _wireframe;
    UINT _descriptor_size = 0;
};
