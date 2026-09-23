#pragma once

#include "../Common/stdafx.h"

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
    bool create_pipeline();
    bool create_geometry();
    bool create_depth_buffer();
    bool create_constant_buffer();
    bool create_upload_buffer(const void* data, UINT64 size, ComPtr<ID3D12Resource>& resource);
    bool create_fence();
    void render_frame();
    void wait_for_gpu();
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
    ComPtr<ID3D12CommandAllocator> _command_allocator;
    ComPtr<ID3D12GraphicsCommandList> _command_list;
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _grid_pipeline_state;
    ComPtr<ID3D12PipelineState> _cube_pipeline_state;
    ComPtr<ID3D12Resource> _grid_vertex_buffer;
    ComPtr<ID3D12Resource> _cube_vertex_buffer;
    ComPtr<ID3D12Resource> _cube_index_buffer;
    D3D12_VERTEX_BUFFER_VIEW _grid_vertex_buffer_view{};
    D3D12_VERTEX_BUFFER_VIEW _cube_vertex_buffer_view{};
    D3D12_INDEX_BUFFER_VIEW _cube_index_buffer_view{};
    ComPtr<ID3D12Resource> _depth_buffer;
    ComPtr<ID3D12DescriptorHeap> _depth_stencil_heap;
    UINT _depth_stencil_descriptor_size = 0;
    ComPtr<ID3D12Resource> _constant_buffer;
    UINT8* _constant_buffer_data = nullptr;
    ComPtr<ID3D12Fence> _fence;
    UINT64 _fence_value = 0;
    HANDLE _fence_event = nullptr;
    UINT _frame_index = 0;
};
