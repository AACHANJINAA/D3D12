#include "../Common/stdafx.h"
#include "Renderer.h"

namespace
{
    struct VERTEX
    {
        float position[3];
        float color[4];
    };

    constexpr VERTEX cube_vertices[] =
    {
        { { -0.45f, -0.45f, 0.25f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -0.45f,  0.45f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  0.45f,  0.45f, 0.25f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.45f, -0.45f, 0.25f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.45f, -0.45f, 0.75f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { { -0.45f,  0.45f, 0.75f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { {  0.45f,  0.45f, 0.75f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  0.45f, -0.45f, 0.75f }, { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    constexpr uint16_t cube_indices[] =
    {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        0, 3, 7, 0, 7, 4
    };

}

constexpr wchar_t RENDERER::window_class_name[];

RENDERER::~RENDERER()
{
    shutdown();
}

bool RENDERER::initialize(HINSTANCE instance, int show_command)
{
    _instance = instance;
    if (!register_window_class() || !create_window(show_command))
    {
        return false;
    }

    return initialize_device() && create_command_objects() && create_swap_chain() &&
        create_render_targets() && _pipeline.initialize(_device.Get()) && create_vertex_buffer() &&
        create_constant_buffer() && create_fence();
}

int RENDERER::run()
{
    MSG message{};
    while (true)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0)
        {
            if (message.message == WM_QUIT)
            {
                return static_cast<int>(message.wParam);
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        render_frame();
    }
}

bool RENDERER::register_window_class()
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.hInstance = _instance;
    window_class.lpfnWndProc = window_procedure;
    window_class.lpszClassName = window_class_name;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (RegisterClassExW(&window_class) != 0)
    {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool RENDERER::create_window(int show_command)
{
    constexpr DWORD window_style = WS_OVERLAPPEDWINDOW;
    RECT client_rect{ 0, 0, 1280, 720 };
    AdjustWindowRect(&client_rect, window_style, FALSE);
    _window = CreateWindowExW(0, window_class_name, L"D3D12 Renderer", window_style,
        CW_USEDEFAULT, CW_USEDEFAULT, client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top, nullptr, nullptr, _instance, this);
    if (_window == nullptr)
    {
        return false;
    }
    ShowWindow(_window, show_command);
    UpdateWindow(_window);
    return true;
}

bool RENDERER::initialize_device()
{
    UINT factory_flags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
    {
        debug_controller->EnableDebugLayer();
        factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&_factory))))
    {
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapter_index = 0;
        _factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND;
        ++adapter_index)
    {
        DXGI_ADAPTER_DESC1 adapter_description{};
        adapter->GetDesc1(&adapter_description);
        if ((adapter_description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device))))
        {
            return true;
        }
    }
    return SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));
}

bool RENDERER::create_command_objects()
{
    CD3DX12_COMMAND_QUEUE_DESC queue_description(D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (FAILED(_device->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&_command_queue))) 
        || FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocator))) 
        || FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(&_command_list))))
    {
        return false;
    }
    return SUCCEEDED(_command_list->Close());
}

bool RENDERER::create_swap_chain()
{
    DXGI_SWAP_CHAIN_DESC1 swap_chain_description{};
    swap_chain_description.BufferCount = frame_count;
    swap_chain_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_description.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap_chain;
    if (FAILED(_factory->CreateSwapChainForHwnd(_command_queue.Get(), _window,
        &swap_chain_description, nullptr, nullptr, &swap_chain)) ||
        FAILED(_factory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER)) ||
        FAILED(swap_chain.As(&_swap_chain)))
    {
        return false;
    }
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    return true;
}

bool RENDERER::create_render_targets()
{
    CD3DX12_DESCRIPTOR_HEAP_DESC heap_description(frame_count, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (FAILED(_device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&_render_target_heap))))
    {
        return false;
    }
    _render_target_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto descriptor_handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT buffer_index = 0; buffer_index < frame_count; ++buffer_index)
    {
        if (FAILED(_swap_chain->GetBuffer(buffer_index, IID_PPV_ARGS(&_render_targets[buffer_index]))))
        {
            return false;
        }
        _device->CreateRenderTargetView(_render_targets[buffer_index].Get(), nullptr, descriptor_handle);
        descriptor_handle.ptr += _render_target_descriptor_size;
    }
    return true;
}

bool RENDERER::create_vertex_buffer()
{
    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC resource_description = CD3DX12_RESOURCE_DESC::buffer(sizeof(cube_vertices));
    if (FAILED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_vertex_buffer))))
    {
        return false;
    }
    void* mapped_data = nullptr;
    if (FAILED(_vertex_buffer->Map(0, nullptr, &mapped_data)))
    {
        return false;
    }
    std::memcpy(mapped_data, cube_vertices, sizeof(cube_vertices));
    _vertex_buffer->Unmap(0, nullptr);
    _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
    _vertex_buffer_view.StrideInBytes = sizeof(VERTEX);
    _vertex_buffer_view.SizeInBytes = sizeof(cube_vertices);

    const CD3DX12_RESOURCE_DESC index_description = CD3DX12_RESOURCE_DESC::buffer(sizeof(cube_indices));
    if (FAILED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &index_description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_index_buffer))))
    {
        return false;
    }

    INPUT_MANAGER::get_instance().initialize(_window);
    CAMERA_MANAGER::get_instance().initialize();
    mapped_data = nullptr;
    if (FAILED(_index_buffer->Map(0, nullptr, &mapped_data)))
    {
        return false;
    }
    std::memcpy(mapped_data, cube_indices, sizeof(cube_indices));
    _index_buffer->Unmap(0, nullptr);
    _index_buffer_view.BufferLocation = _index_buffer->GetGPUVirtualAddress();
    _index_buffer_view.SizeInBytes = sizeof(cube_indices);
    _index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
    return true;
}

bool RENDERER::create_constant_buffer()
{
    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::buffer(
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (FAILED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_constant_buffer))))
    {
        return false;
    }

    void* mapped_data = nullptr;
    if (FAILED(_constant_buffer->Map(0, nullptr, &mapped_data)))
    {
        return false;
    }
    _constant_data = static_cast<UINT8*>(mapped_data);
    return true;
}

bool RENDERER::create_fence()
{
    if (FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence))))
    {
        return false;
    }
    _fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return _fence_event != nullptr;
}

void RENDERER::render_frame()
{
    static ULONGLONG last_frame_time = GetTickCount64();
    const ULONGLONG current_time = GetTickCount64();
    const float delta_time = (std::min)(
        static_cast<float>(current_time - last_frame_time) / 1000.0f, 0.1f);
    last_frame_time = current_time;
    INPUT_MANAGER::get_instance().update();
    CAMERA_MANAGER::get_instance().update(delta_time);
    const MATH::MATRIX4X4 transform = CAMERA_MANAGER::get_instance().get_view_projection(1280.0f / 720.0f);
    std::memcpy(_constant_data, &transform, sizeof(transform));

    _command_allocator->Reset();
    _command_list->Reset(_command_allocator.Get(), _pipeline.get_pipeline());
    
    const CD3DX12_RESOURCE_BARRIER to_render_target = CD3DX12_RESOURCE_BARRIER::transition(
        _render_targets[_frame_index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    _command_list->ResourceBarrier(1, &to_render_target);
    
    auto render_target_handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    render_target_handle.ptr += static_cast<SIZE_T>(_frame_index) * _render_target_descriptor_size;
    _command_list->OMSetRenderTargets(1, &render_target_handle, FALSE, nullptr);
    constexpr float clear_color[] = { 0.05f, 0.05f, 0.08f, 1.0f };
    _command_list->ClearRenderTargetView(render_target_handle, clear_color, 0, nullptr);
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor_rect{ 0, 0, 1280, 720 };
    _command_list->RSSetViewports(1, &viewport);
    _command_list->RSSetScissorRects(1, &scissor_rect);
    _command_list->SetGraphicsRootSignature(_pipeline.get_root_signature());
    _command_list->SetGraphicsRootConstantBufferView(0, _constant_buffer->GetGPUVirtualAddress());
    _command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _command_list->IASetVertexBuffers(0, 1, &_vertex_buffer_view);
    _command_list->IASetIndexBuffer(&_index_buffer_view);
    _command_list->DrawIndexedInstanced(36, 1, 0, 0, 0);
    _command_list->SetPipelineState(_pipeline.get_wireframe_pipeline());
    _command_list->DrawIndexedInstanced(36, 1, 0, 0, 0);
    const CD3DX12_RESOURCE_BARRIER to_present = CD3DX12_RESOURCE_BARRIER::transition(
        _render_targets[_frame_index].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    _command_list->ResourceBarrier(1, &to_present);
    _command_list->Close();
    ID3D12CommandList* command_lists[] = { _command_list.Get() };
    _command_queue->ExecuteCommandLists(1, command_lists);
    _swap_chain->Present(1, 0);
    move_to_next_frame();
}

void RENDERER::move_to_next_frame()
{
    const UINT64 current_fence_value = ++_fence_value;
    _command_queue->Signal(_fence.Get(), current_fence_value);
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    if (_fence->GetCompletedValue() < current_fence_value)
    {
        _fence->SetEventOnCompletion(current_fence_value, _fence_event);
        WaitForSingleObject(_fence_event, INFINITE);
    }
}

void RENDERER::wait_for_gpu()
{
    if (_command_queue == nullptr || _fence == nullptr || _fence_event == nullptr)
    {
        return;
    }
    const UINT64 fence_value = ++_fence_value;
    _command_queue->Signal(_fence.Get(), fence_value);
    _fence->SetEventOnCompletion(fence_value, _fence_event);
    WaitForSingleObject(_fence_event, INFINITE);
}

void RENDERER::shutdown()
{
    wait_for_gpu();
    if (_fence_event != nullptr)
    {
        CloseHandle(_fence_event);
        _fence_event = nullptr;
    }
    if (_constant_buffer != nullptr && _constant_data != nullptr)
    {
        _constant_buffer->Unmap(0, nullptr);
        _constant_data = nullptr;
    }
    _constant_buffer.Reset();
    _vertex_buffer.Reset();
    _index_buffer.Reset();
    _command_list.Reset();
    _command_allocator.Reset();
    for (auto& render_target : _render_targets) render_target.Reset();
    _render_target_heap.Reset();
    _swap_chain.Reset();
    _command_queue.Reset();
    _device.Reset();
    _factory.Reset();
    if (_window != nullptr)
    {
        DestroyWindow(_window);
        _window = nullptr;
    }
    if (_instance != nullptr)
    {
        UnregisterClassW(window_class_name, _instance);
        _instance = nullptr;
    }
}

LRESULT CALLBACK RENDERER::window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return TRUE;
    }
    INPUT_MANAGER::get_instance().process_message(message, wparam);
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
