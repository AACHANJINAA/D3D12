#pragma once

#include "../Common/stdafx.h"
#include "Manager/CameraManager.h"
#include "Manager/InputManager.h"
#include "Manager/LightManager.h"
#include "Manager/ResourceManager.h"
#include "SceneRenderData.h"
#include "Pass/GBufferRenderPass.h"
#include "Pass/DeferredLightPass.h"
#include "Pass/SkyboxRenderPass.h"
#include "../Resource/ModelResource.h"
#include <memory>
#include "../Resource/Texture.h"
#include "../UI/ViewerUi.h"
#include "../../Benchmark/BenchmarkScene.h"

class RENDERER
{
public:
    RENDERER() = default;
    ~RENDERER();
    RENDERER(const RENDERER&) = delete;
    RENDERER& operator=(const RENDERER&) = delete;

    bool initialize(HINSTANCE instance, int show_command);
    int run();

private:
    static constexpr UINT frame_count = 2;
    static constexpr wchar_t window_class_name[] = L"D3D12 Engine Renderer";
    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    bool register_window_class();
    bool create_window(int show_command);
    bool initialize_device();
    bool create_command_objects();
    bool create_swap_chain();
    bool create_render_targets();
    bool create_gbuffer_targets();
    bool update_deferred_resources();
    bool resize_swap_chain();
    bool import_model(const VIEWER_REQUEST& request);
    void process_scene_request();
    void update_previews();
    void focus_object(SCENE_OBJECT& object);
    bool create_fence();
    bool create_texture();
    void render_frame();
    void toggle_fullscreen();
    bool wait_for_gpu();
    void move_to_next_frame();
    void shutdown();

    HINSTANCE _instance = nullptr;
    HWND _window = nullptr;
    ComPtr<IDXGIFactory4> _factory;
    ComPtr<ID3D12Device> _device;
    ComPtr<ID3D12CommandQueue> _command_queue;
    ComPtr<IDXGISwapChain3> _swap_chain;
    ComPtr<ID3D12DescriptorHeap> _render_target_heap;
    std::array<ComPtr<ID3D12Resource>, frame_count> _render_targets;
    UINT _render_target_descriptor_size = 0;
    ComPtr<ID3D12DescriptorHeap> _depth_stencil_heap;
    ComPtr<ID3D12Resource> _depth_stencil_buffer;
    ComPtr<ID3D12CommandAllocator> _command_allocator;
    ComPtr<ID3D12GraphicsCommandList> _command_list;
    GBUFFER_RENDER_PASS _gbuffer_render_pass;
    DEFERRED_LIGHT_PASS _deferred_light_pass;
    SKYBOX_RENDER_PASS _skybox_render_pass;
    SCENE _scene;
    RESOURCE_MANAGER _resources;
    SCENE_RENDER_DATA _render_data;
    std::shared_ptr<MODEL_RESOURCE> _failed_import;
    ComPtr<ID3D12CommandAllocator> _failed_import_allocator;
    ComPtr<ID3D12GraphicsCommandList> _failed_import_list;
    TEXTURE_SET _material_textures;
    VIEWER_UI _ui;
    VIEWER_SETTINGS _viewer_settings;
    BENCHMARK_SCENE _benchmark;
    ComPtr<ID3D12Fence> _fence;
    UINT64 _fence_value = 0;
    HANDLE _fence_event = nullptr;
    UINT _frame_index = 0;
    bool _is_com_initialized = false;
    bool _is_fullscreen = false;
    bool _is_render_failed = false;
    LONG_PTR _windowed_style = WS_OVERLAPPEDWINDOW;
    LONG_PTR _windowed_ex_style = WS_EX_APPWINDOW;
    RECT _windowed_rect{};
};
