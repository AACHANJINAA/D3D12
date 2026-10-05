#pragma once
#include "../Scene/Scene.h"
#include "ViewerSettings.h"
#include "Pass/GBufferRenderPass.h"

class SCENE_RENDER_DATA
{
public:
    SCENE_RENDER_DATA() = default;
    SCENE_RENDER_DATA(const SCENE_RENDER_DATA&) = delete;
    SCENE_RENDER_DATA& operator=(const SCENE_RENDER_DATA&) = delete;
    ~SCENE_RENDER_DATA() { reset(); }
    bool update(ID3D12Device* device, const SCENE& scene, const VIEWER_SETTINGS& settings,
        const MATH::MATRIX4X4& view_projection, const MATH::VECTOR3& camera, bool isinstancing = true);
    const std::vector<GBUFFER_DRAW>& get_draws() const { return _draws; }
    size_t get_instance_count() const { return _instance_count; }
    D3D12_GPU_VIRTUAL_ADDRESS get_lighting_constants() const { return _buffer->GetGPUVirtualAddress(); }
    void reset();
private:
    bool reserve(ID3D12Device* device, size_t bytes);
    ComPtr<ID3D12Resource> _buffer;
    UINT8* _mapped = nullptr;
    size_t _capacity = 0;
    size_t _instance_count = 0;
    std::vector<GBUFFER_DRAW> _draws;
};
