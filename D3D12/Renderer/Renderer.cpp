#include "../Common/stdafx.h"
#include "Renderer.h"

namespace
{
    bool report_initialization_failure(const wchar_t* stage)
    {
        OutputDebugStringW(L"D3D12 initialization failed: ");
        OutputDebugStringW(stage);
        OutputDebugStringW(L"\n");
        MessageBoxW(nullptr, stage, L"D3D12 Initialization Error", MB_OK | MB_ICONERROR);
        return false;
    }

    struct FRAME_DATA
    {
        MATH::MATRIX4X4 transform;
        MATH::VECTOR3 light_direction;
        float light_intensity = 0.0f;
        MATH::VECTOR3 light_color;
        float padding = 0.0f;
        MATH::VECTOR3 camera_position;
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ambient_strength = 0.03f;
        float reserved[2]{};
    };

    static_assert(sizeof(FRAME_DATA) <= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    std::filesystem::path get_asset_path(const wchar_t* relative_path)
    {
        wchar_t module_path[MAX_PATH]{};
        const DWORD path_length = GetModuleFileNameW(nullptr, module_path, _countof(module_path));
        if (path_length == 0)
        {
            return {};
        }
        return std::filesystem::path(std::wstring(module_path, path_length)).parent_path() /
            L"Asset" / relative_path;
    }

}

constexpr wchar_t RENDERER::window_class_name[];

RENDERER::~RENDERER()
{
    shutdown();
}

bool RENDERER::initialize(HINSTANCE instance, int show_command)
{
    _instance = instance;
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE)
    {
        return false;
    }
    _is_com_initialized = SUCCEEDED(com_result);
    if (!register_window_class() || !create_window(show_command))
    {
        return false;
    }

    if (!initialize_device()) return report_initialization_failure(L"initialize_device");
    if (!create_command_objects()) return report_initialization_failure(L"create_command_objects");
    if (!create_swap_chain()) return report_initialization_failure(L"create_swap_chain");
    if (!create_render_targets()) return report_initialization_failure(L"create_render_targets");
    if (!_mesh_render_pass.initialize(_device.Get()))
        return report_initialization_failure(L"mesh_render_pass.initialize");
    if (!_skybox_render_pass.initialize(_device.Get()))
        return report_initialization_failure(L"skybox_render_pass.initialize");
    if (!create_vertex_buffer()) return report_initialization_failure(L"create_vertex_buffer");
    if (!create_constant_buffer()) return report_initialization_failure(L"create_constant_buffer");
    if (!create_fence()) return report_initialization_failure(L"create_fence");
    if (!create_texture()) return report_initialization_failure(L"create_texture");
    return true;
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

    CD3DX12_DESCRIPTOR_HEAP_DESC depth_heap_description(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    if (FAILED(_device->CreateDescriptorHeap(&depth_heap_description,
        IID_PPV_ARGS(&_depth_stencil_heap))))
    {
        return false;
    }

    const CD3DX12_HEAP_PROPERTIES depth_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC depth_description{};
    depth_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_description.Width = 1280;
    depth_description.Height = 720;
    depth_description.DepthOrArraySize = 1;
    depth_description.MipLevels = 1;
    depth_description.Format = DXGI_FORMAT_D32_FLOAT;
    depth_description.SampleDesc.Count = 1;
    depth_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depth_clear_value{};
    depth_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
    depth_clear_value.DepthStencil.Depth = 1.0f;
    depth_clear_value.DepthStencil.Stencil = 0;

    if (FAILED(_device->CreateCommittedResource(
        &depth_heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &depth_description,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depth_clear_value,
        IID_PPV_ARGS(&_depth_stencil_buffer))))
    {
        return false;
    }

    _device->CreateDepthStencilView(
        _depth_stencil_buffer.Get(),
        nullptr,
        _depth_stencil_heap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool RENDERER::create_vertex_buffer()
{
    if (!_gltf_mesh.load(get_asset_path(L"Mesh/DamagedHelmet/DamagedHelmet.gltf")))
    {
        return false;
    }

    const auto& vertices = _gltf_mesh.get_vertices();
    const auto& indices = _gltf_mesh.get_indices();
    if (vertices.empty() || indices.empty())
    {
        return false;
    }

    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC resource_description = CD3DX12_RESOURCE_DESC::buffer(
        sizeof(GLTF_VERTEX) * vertices.size());
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
    std::memcpy(mapped_data, vertices.data(), sizeof(GLTF_VERTEX) * vertices.size());
    _vertex_buffer->Unmap(0, nullptr);
    _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
    _vertex_buffer_view.StrideInBytes = sizeof(GLTF_VERTEX);
    _vertex_buffer_view.SizeInBytes = static_cast<UINT>(sizeof(GLTF_VERTEX) * vertices.size());

    const CD3DX12_RESOURCE_DESC index_description = CD3DX12_RESOURCE_DESC::buffer(
        sizeof(uint32_t) * indices.size());
    if (FAILED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &index_description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_index_buffer))))
    {
        return false;
    }

    INPUT_MANAGER::get_instance().initialize(_window);
    CAMERA_MANAGER::get_instance().initialize();
    LIGHT_MANAGER::get_instance().initialize();
    mapped_data = nullptr;
    if (FAILED(_index_buffer->Map(0, nullptr, &mapped_data)))
    {
        return false;
    }
    std::memcpy(mapped_data, indices.data(), sizeof(uint32_t) * indices.size());
    _index_buffer->Unmap(0, nullptr);
    _index_buffer_view.BufferLocation = _index_buffer->GetGPUVirtualAddress();
    _index_buffer_view.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    _index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
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

bool RENDERER::create_texture()
{
    if (FAILED(_command_allocator->Reset()) ||
        FAILED(_command_list->Reset(_command_allocator.Get(), _mesh_render_pass.get_pipeline())))
    {
        return false;
    }

    const std::array<std::filesystem::path, TEXTURE_SET::texture_count> texture_paths =
    {
        get_asset_path(L"Mesh/DamagedHelmet/Default_albedo.jpg"),
        get_asset_path(L"Mesh/DamagedHelmet/Default_normal.jpg"),
        get_asset_path(L"Mesh/DamagedHelmet/Default_metalRoughness.jpg"),
        get_asset_path(L"Mesh/DamagedHelmet/Default_AO.jpg"),
        get_asset_path(L"Mesh/DamagedHelmet/Default_emissive.jpg")
    };

    if (!_material_textures.initialize(
        _device.Get(),
        _command_list.Get(),
        texture_paths))
    {
        return false;
    }

    if (!_skybox_render_pass.load_cubemap(
        _device.Get(), _command_list.Get(),
        get_asset_path(L"Skybox/cloudy/cloudy_skybox.dds")))
    {
        return false;
    }

    if (FAILED(_command_list->Close()))
    {
        return false;
    }
    ID3D12CommandList* command_lists[] = { _command_list.Get() };
    _command_queue->ExecuteCommandLists(1, command_lists);
    move_to_next_frame();
    return true;
}

void RENDERER::render_frame()
{
    static ULONGLONG last_frame_time = GetTickCount64();
    const ULONGLONG current_time = GetTickCount64();
    const float delta_time = (std::min)(
        static_cast<float>(current_time - last_frame_time) / 1000.0f, 0.1f);
    last_frame_time = current_time;
    INPUT_MANAGER::get_instance().update();
    static bool was_light_orbit_key_down = false;
    const bool is_light_orbit_key_down = INPUT_MANAGER::get_instance().is_key_down('L');
    if (is_light_orbit_key_down && !was_light_orbit_key_down)
    {
        LIGHT_MANAGER::get_instance().toggle_orbit();
    }
    was_light_orbit_key_down = is_light_orbit_key_down;
    LIGHT_MANAGER::get_instance().update(delta_time);
    CAMERA_MANAGER::get_instance().update(delta_time);
    const MATH::MATRIX4X4 transform = CAMERA_MANAGER::get_instance().get_view_projection(1280.0f / 720.0f);
    const DIRECTIONAL_LIGHT& directional_light =
        LIGHT_MANAGER::get_instance().get_directional_light();
    FRAME_DATA frame_data{};
    frame_data.transform = transform;
    frame_data.light_direction = directional_light.direction;
    frame_data.light_intensity = directional_light.intensity;
    frame_data.light_color = directional_light.color;
    frame_data.camera_position = CAMERA_MANAGER::get_instance().get_position();
    frame_data.metallic = 0.85f;
    frame_data.roughness = 0.35f;
    frame_data.ambient_strength = 0.03f;
    std::memcpy(_constant_data, &frame_data, sizeof(frame_data));

    _command_allocator->Reset();
    _command_list->Reset(_command_allocator.Get(), _mesh_render_pass.get_pipeline());
    
    const CD3DX12_RESOURCE_BARRIER to_render_target = CD3DX12_RESOURCE_BARRIER::transition(
        _render_targets[_frame_index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    _command_list->ResourceBarrier(1, &to_render_target);
    
    auto render_target_handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    render_target_handle.ptr += static_cast<SIZE_T>(_frame_index) * _render_target_descriptor_size;
    const auto depth_stencil_handle = _depth_stencil_heap->GetCPUDescriptorHandleForHeapStart();
    _command_list->OMSetRenderTargets(1, &render_target_handle, FALSE, &depth_stencil_handle);
    constexpr float clear_color[] = { 0.05f, 0.05f, 0.08f, 1.0f };
    _command_list->ClearRenderTargetView(render_target_handle, clear_color, 0, nullptr);
    _command_list->ClearDepthStencilView(
        depth_stencil_handle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor_rect{ 0, 0, 1280, 720 };
    _command_list->RSSetViewports(1, &viewport);
    _command_list->RSSetScissorRects(1, &scissor_rect);
    _skybox_render_pass.render(_command_list.Get());
    _command_list->SetPipelineState(_mesh_render_pass.get_pipeline());
    _command_list->SetGraphicsRootSignature(_mesh_render_pass.get_root_signature());
    _command_list->SetGraphicsRootConstantBufferView(0, _constant_buffer->GetGPUVirtualAddress());
    ID3D12DescriptorHeap* descriptor_heaps[] = { _material_textures.get_srv_heap() };
    _command_list->SetDescriptorHeaps(1, descriptor_heaps);
    _command_list->SetGraphicsRootDescriptorTable(1, _material_textures.get_gpu_handle());
    _command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _command_list->IASetVertexBuffers(0, 1, &_vertex_buffer_view);
    _command_list->IASetIndexBuffer(&_index_buffer_view);
    _command_list->DrawIndexedInstanced(
        static_cast<UINT>(_gltf_mesh.get_indices().size()), 1, 0, 0, 0);
    _command_list->SetPipelineState(_mesh_render_pass.get_wireframe_pipeline());
    _command_list->DrawIndexedInstanced(
        static_cast<UINT>(_gltf_mesh.get_indices().size()), 1, 0, 0, 0);
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
    _depth_stencil_buffer.Reset();
    _depth_stencil_heap.Reset();
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
    if (_is_com_initialized)
    {
        CoUninitialize();
        _is_com_initialized = false;
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
