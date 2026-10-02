#include "../Common/stdafx.h"
#include "Renderer.h"
#include <roapi.h>

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
    const HRESULT com_result = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE)
    {
        return false;
    }
    _is_com_initialized = SUCCEEDED(com_result);
    std::string benchmark_error;
    if (!_benchmark.load_arguments(benchmark_error))
    {
        MessageBoxA(nullptr, benchmark_error.c_str(), "Benchmark configuration", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!register_window_class() || !create_window(show_command))
    {
        return false;
    }

    if (!initialize_device()) return report_initialization_failure(L"initialize_device");
    if (!create_command_objects()) return report_initialization_failure(L"create_command_objects");
    if (!create_swap_chain()) return report_initialization_failure(L"create_swap_chain");
    if (!create_render_targets()) return report_initialization_failure(L"create_render_targets");
    if (!create_gbuffer_targets()) return report_initialization_failure(L"create_gbuffer_targets");
    if (!_gbuffer_render_pass.initialize(_device.Get()))
        return report_initialization_failure(L"gbuffer_render_pass.initialize");
    if (!_deferred_light_pass.initialize(_device.Get()))
        return report_initialization_failure(L"deferred_light_pass.initialize");
    if (!_skybox_render_pass.initialize(_device.Get()))
        return report_initialization_failure(L"skybox_render_pass.initialize");
    INPUT_MANAGER::get_instance().initialize(_window);
    CAMERA_MANAGER::get_instance().initialize();
    LIGHT_MANAGER::get_instance().initialize();
    if (!create_fence()) return report_initialization_failure(L"create_fence");
    if (!create_texture()) return report_initialization_failure(L"create_texture");
    if (!update_deferred_resources()) return report_initialization_failure(L"deferred_resources");
    if (!_ui.initialize(_window, _device.Get(), _command_queue.Get(), frame_count))
        return report_initialization_failure(L"viewer_ui.initialize");
    if (_benchmark.isenabled)
    {
        for (const auto& object : _benchmark.objects)
            if (!import_model(object)) return false;
        _scene.select(0);
        _benchmark.apply(_viewer_settings);
        CAMERA_MANAGER::get_instance().set_target_bounds({}, 1.0f);
        CAMERA_MANAGER::get_instance().set_pose(_benchmark.camera_position, _benchmark.camera_target);
    }
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
        if (IsIconic(_window))
        {
            WaitMessage();
            continue;
        }
        render_frame();
        if (_is_render_failed) return EXIT_FAILURE;
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
    RECT client_rect{ 0, 0, _benchmark.isenabled ? static_cast<LONG>(_benchmark.width) : 1280,
        _benchmark.isenabled ? static_cast<LONG>(_benchmark.height) : 720 };
    AdjustWindowRect(&client_rect, window_style, FALSE);
    _window = CreateWindowExW(0, window_class_name, L"Deffered - D3D12 Renderer", window_style,
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

bool RENDERER::resize_swap_chain()
{
    if (!wait_for_gpu()) return false;
    // Discard recorded references to the old render targets after GPU completion.
    if (FAILED(_command_allocator->Reset()) ||
        FAILED(_command_list->Reset(_command_allocator.Get(), nullptr)) ||
        FAILED(_command_list->Close()))
        return false;
    for (auto& render_target : _render_targets)
    {
        render_target.Reset();
    }
    _gbuffer_render_pass.release_targets();
    _depth_stencil_buffer.Reset();
    _render_target_heap.Reset();
    _depth_stencil_heap.Reset();

    RECT client_rect{};
    GetClientRect(_window, &client_rect);
    const UINT width = static_cast<UINT>((std::max)(client_rect.right - client_rect.left, 1L));
    const UINT height = static_cast<UINT>((std::max)(client_rect.bottom - client_rect.top, 1L));
    if (FAILED(_swap_chain->ResizeBuffers(
        frame_count, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
    {
        return false;
    }
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    return create_render_targets() && create_gbuffer_targets() && update_deferred_resources();
}

void RENDERER::toggle_fullscreen()
{
    if (_window == nullptr || _swap_chain == nullptr)
    {
        return;
    }

    if (!_is_fullscreen)
    {
        GetWindowRect(_window, &_windowed_rect);
        _windowed_style = GetWindowLongPtrW(_window, GWL_STYLE);
        _windowed_ex_style = GetWindowLongPtrW(_window, GWL_EXSTYLE);

        MONITORINFO monitor_info{ sizeof(MONITORINFO) };
        HMONITOR monitor = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfoW(monitor, &monitor_info);
        SetWindowLongPtrW(_window, GWL_STYLE, WS_POPUP);
        SetWindowLongPtrW(_window, GWL_EXSTYLE, WS_EX_APPWINDOW);
        SetWindowPos(_window, HWND_TOP,
            monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
            monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
            monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        _is_fullscreen = true;
    }
    else
    {
        SetWindowLongPtrW(_window, GWL_STYLE, _windowed_style);
        SetWindowLongPtrW(_window, GWL_EXSTYLE, _windowed_ex_style);
        SetWindowPos(_window, HWND_TOP,
            _windowed_rect.left, _windowed_rect.top,
            _windowed_rect.right - _windowed_rect.left,
            _windowed_rect.bottom - _windowed_rect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        _is_fullscreen = false;
    }
    if (!resize_swap_chain())
    {
        _is_render_failed = true;
        report_initialization_failure(L"resize_swap_chain");
    }
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
    RECT client_rect{};
    GetClientRect(_window, &client_rect);
    const UINT client_width = static_cast<UINT>(client_rect.right - client_rect.left);
    const UINT client_height = static_cast<UINT>(client_rect.bottom - client_rect.top);
    depth_description.Width = client_width;
    depth_description.Height = client_height;
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

bool RENDERER::create_gbuffer_targets()
{
    const auto description = _render_targets[0]->GetDesc();
    return _gbuffer_render_pass.resize(_device.Get(),
        static_cast<UINT>(description.Width), description.Height);
}

bool RENDERER::update_deferred_resources()
{
    return _deferred_light_pass.set_resources(_device.Get(), _gbuffer_render_pass,
        _skybox_render_pass.get_cubemap(), _material_textures.get_brdf_lut(),
        _material_textures.get_specular_cubemap());
}

void RENDERER::focus_object(SCENE_OBJECT& object)
{
    RECT client{};
    GetClientRect(_window, &client);
    const float aspect = static_cast<float>((std::max)(client.right, 1L)) /
        static_cast<float>((std::max)(client.bottom, 1L));
    CAMERA_MANAGER::get_instance().frame_model(object.get_center(), object.get_radius(), aspect);
    _viewer_settings.depth_range = object.get_radius() * 8;
}

void RENDERER::update_previews()
{
    std::array<ID3D12Resource*, 5> previews{};
    if (const auto* object = _scene.selected())
    {
        const auto& materials = object->model->get_materials();
        if (object->material_index < materials.size())
            for (size_t index = 0; index < previews.size(); ++index)
                previews[index] = materials[object->material_index]->get_textures().get_texture(index);
    }
    _ui.set_textures(_device.Get(), previews);
}

void RENDERER::process_scene_request()
{
    const auto request = _ui.take_request();
    if (request.action == VIEWER_ACTION::open || request.action == VIEWER_ACTION::add)
        import_model(request);
    else if (request.action == VIEWER_ACTION::erase)
    {
        if (!wait_for_gpu()) { _is_render_failed = true; return; }
        _scene.erase(request.object_id);
        _resources.prune();
    }
    else if (request.action == VIEWER_ACTION::focus)
        if (auto* object = _scene.find(request.object_id)) focus_object(*object);
    _resources.prune();
}

bool RENDERER::import_model(const VIEWER_REQUEST& request)
{
    const auto& path = request.path;
    const UINT64 replace_id = request.action == VIEWER_ACTION::open ? request.object_id : 0;
    if (path.empty()) return false;
    if (!request.has_valid_transform())
    {
        _ui.show_import_error("Invalid import transform.");
        return false;
    }
    try
    {
        if (!replace_id && _scene.get_objects().size() >= SCENE::object_limit)
        {
            _ui.show_import_error("Scene object limit reached.");
            return false;
        }
        if (replace_id && !_scene.find(replace_id)) return false;
        auto candidate = _resources.find_model(path);
        if (!candidate)
        {
            candidate = std::make_shared<MODEL_RESOURCE>();
            ComPtr<ID3D12CommandAllocator> allocator;
            ComPtr<ID3D12GraphicsCommandList> list;
            if (FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
                FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))))
            {
                _ui.show_import_error("Could not create model upload commands.");
                return false;
            }
            std::string error;
            if (!candidate->load(_device.Get(), list.Get(), path, _resources.get_texture_cache(), error))
            {
                list->Close();
                _ui.show_import_error(error);
                return false;
            }
            if (FAILED(list->Close()))
            {
                _ui.show_import_error("Could not finish model upload commands.");
                return false;
            }
            ID3D12CommandList* lists[] = { list.Get() };
            _command_queue->ExecuteCommandLists(1, lists);
            if (!wait_for_gpu())
            {
                _failed_import = std::move(candidate);
                _failed_import_allocator = std::move(allocator);
                _failed_import_list = std::move(list);
                _is_render_failed = true;
                _ui.show_import_error("GPU upload synchronization failed. Rendering has stopped.");
                return false;
            }
            candidate->release_uploads();
            _resources.publish_model(path, candidate);
        }
        // Cached models may still be referenced by the last submitted frame.
        if (!wait_for_gpu()) { _is_render_failed = true; return false; }
        if (replace_id) _scene.replace(replace_id, candidate);
        else _scene.add(candidate);
        if (auto* object = _scene.selected())
        {
            object->position = request.position;
            object->rotation = request.rotation;
            object->scale = request.scale;
            focus_object(*object);
        }
        _resources.prune();
        return true;
    }
    catch (const std::exception& error)
    {
        _resources.prune();
        _ui.show_import_error(error.what());
        return false;
    }
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
        FAILED(_command_list->Reset(_command_allocator.Get(), nullptr)))
    {
        return false;
    }

    const std::array<std::filesystem::path, TEXTURE_SET::texture_count> texture_paths{};

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

    if (!_material_textures.append_cubemap(
        _device.Get(), _skybox_render_pass.get_cubemap()))
    {
        return false;
    }
    if (!_material_textures.append_brdf_lut(
        _device.Get(), _command_list.Get(),
        get_asset_path(L"Skybox/BRDF.dds")))
    {
        return false;
    }
    if (!_material_textures.append_specular_cubemap(
        _device.Get(), _command_list.Get(),
        get_asset_path(L"Skybox/cloudy/cloudy_specular.dds")))
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
    if (_is_render_failed) return false;
    _material_textures.release_uploads();
    return true;
}

void RENDERER::render_frame()
{
    if (!_benchmark.isenabled) process_scene_request();
    if (_is_render_failed) return;
    static ULONGLONG last_frame_time = GetTickCount64();
    const ULONGLONG current_time = GetTickCount64();
    const float delta_time = (std::min)(
        static_cast<float>(current_time - last_frame_time) / 1000.0f, 0.1f);
    last_frame_time = current_time;
    if (!_benchmark.isenabled)
    {
    update_previews();
    _ui.begin_frame(_viewer_settings, _scene, { _resources.get_model_count(), _resources.get_texture_count() });
    update_previews();
    INPUT_MANAGER::get_instance().set_ui_capture(_ui.wants_mouse(), _ui.wants_keyboard());
    INPUT_MANAGER::get_instance().update();
    static bool was_fullscreen_key_down = false;
    const bool is_fullscreen_key_down = GetForegroundWindow() == _window &&
        INPUT_MANAGER::get_instance().is_key_down('1');
    if (is_fullscreen_key_down && !was_fullscreen_key_down)
    {
        toggle_fullscreen();
    }
    was_fullscreen_key_down = is_fullscreen_key_down;
    if (_is_render_failed) return;
    LIGHT_MANAGER::get_instance().set_light(_viewer_settings.light_direction,
        _viewer_settings.light_color, _viewer_settings.light_intensity,
        _viewer_settings.is_light_orbiting);
    static bool was_light_orbit_key_down = false;
    const bool is_light_orbit_key_down = INPUT_MANAGER::get_instance().is_key_down('L');
    if (is_light_orbit_key_down && !was_light_orbit_key_down)
    {
        LIGHT_MANAGER::get_instance().toggle_orbit();
    }
    was_light_orbit_key_down = is_light_orbit_key_down;
    LIGHT_MANAGER::get_instance().update(delta_time);
    _viewer_settings.light_direction = LIGHT_MANAGER::get_instance().get_directional_light().direction;
    _viewer_settings.is_light_orbiting = LIGHT_MANAGER::get_instance().is_orbiting();
    if (const auto* selected = _scene.selected())
        CAMERA_MANAGER::get_instance().set_target_bounds(selected->get_center(), selected->get_radius());
    else CAMERA_MANAGER::get_instance().set_target_bounds({}, 1.0f);
    CAMERA_MANAGER::get_instance().update(delta_time);
    }
    float scene_distance = 1.0f;
    for (const auto& object : _scene.get_objects())
        if (object.isvisible) scene_distance = (std::max)(scene_distance,
            MATH::length(MATH::subtract(object.get_center(), CAMERA_MANAGER::get_instance().get_position())) + object.get_radius() * 2);
    CAMERA_MANAGER::get_instance().set_scene_distance(scene_distance);
    RECT client_rect{};
    GetClientRect(_window, &client_rect);
    const float client_width = static_cast<float>(client_rect.right - client_rect.left);
    const float client_height = static_cast<float>(client_rect.bottom - client_rect.top);
    if (client_width <= 0.0f || client_height <= 0.0f) return;
    const auto buffer_description = _render_targets[0]->GetDesc();
    if (buffer_description.Width != static_cast<UINT64>(client_width) ||
        buffer_description.Height != static_cast<UINT>(client_height))
    {
        if (!resize_swap_chain())
        {
            _is_render_failed = true;
            report_initialization_failure(L"resize_swap_chain");
            return;
        }
    }
    const float aspect_ratio = client_width / client_height;
    const MATH::MATRIX4X4 transform =
        CAMERA_MANAGER::get_instance().get_view_projection(aspect_ratio);
    if (!_render_data.update(_device.Get(), _scene, _viewer_settings, transform,
        CAMERA_MANAGER::get_instance().get_position()))
    {
        _is_render_failed = true;
        report_initialization_failure(L"scene_render_data.update");
        return;
    }

    if (FAILED(_command_allocator->Reset()) ||
        FAILED(_command_list->Reset(_command_allocator.Get(), nullptr)))
    {
        _is_render_failed = true;
        report_initialization_failure(L"render_frame.reset");
        return;
    }
    
    const CD3DX12_RESOURCE_BARRIER to_render_target = CD3DX12_RESOURCE_BARRIER::transition(
        _render_targets[_frame_index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    _command_list->ResourceBarrier(1, &to_render_target);
    
    auto render_target_handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    render_target_handle.ptr += static_cast<SIZE_T>(_frame_index) * _render_target_descriptor_size;
    const auto depth_stencil_handle = _depth_stencil_heap->GetCPUDescriptorHandleForHeapStart();
    constexpr float clear_color[] = { 0.22f, 0.23f, 0.25f, 1.0f };
    _command_list->ClearRenderTargetView(render_target_handle, clear_color, 0, nullptr);
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, client_width, client_height, 0.0f, 1.0f };
    D3D12_RECT scissor_rect{
        0, 0, static_cast<LONG>(client_width), static_cast<LONG>(client_height) };
    _command_list->RSSetViewports(1, &viewport);
    _command_list->RSSetScissorRects(1, &scissor_rect);

    _gbuffer_render_pass.render(_command_list.Get(), depth_stencil_handle,
        _render_data.get_draws(), _viewer_settings.mode == VIEW_MODE::wireframe);

    // Full-screen passes have depth disabled; keep geometry depth for future overlays.
    _command_list->OMSetRenderTargets(1, &render_target_handle, FALSE, nullptr);
    if (_viewer_settings.is_skybox_visible)
        _skybox_render_pass.render(_command_list.Get(), _viewer_settings.exposure);
    _deferred_light_pass.render(_command_list.Get(), _render_data.get_lighting_constants());
    if (!_benchmark.isenabled) _ui.render(_command_list.Get());
    const CD3DX12_RESOURCE_BARRIER to_present = CD3DX12_RESOURCE_BARRIER::transition(
        _render_targets[_frame_index].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    _command_list->ResourceBarrier(1, &to_present);
    if (FAILED(_command_list->Close()))
    {
        _is_render_failed = true;
        report_initialization_failure(L"render_frame.close");
        return;
    }
    ID3D12CommandList* command_lists[] = { _command_list.Get() };
    _command_queue->ExecuteCommandLists(1, command_lists);
    if (FAILED(_swap_chain->Present(_benchmark.isenabled ? _benchmark.vsync : 1, 0)))
    {
        _is_render_failed = true;
        report_initialization_failure(L"render_frame.present");
        return;
    }
    move_to_next_frame();
}

void RENDERER::move_to_next_frame()
{
    if (!wait_for_gpu()) _is_render_failed = true;
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
}

bool RENDERER::wait_for_gpu()
{
    if (_command_queue == nullptr || _fence == nullptr || _fence_event == nullptr)
    {
        return true;
    }
    const UINT64 fence_value = ++_fence_value;
    if (FAILED(_command_queue->Signal(_fence.Get(), fence_value)) ||
        FAILED(_fence->SetEventOnCompletion(fence_value, _fence_event))) return false;
    return WaitForSingleObject(_fence_event, INFINITE) == WAIT_OBJECT_0 &&
        SUCCEEDED(_device->GetDeviceRemovedReason());
}

void RENDERER::shutdown()
{
    wait_for_gpu();
    _ui.shutdown();
    if (_fence_event != nullptr)
    {
        CloseHandle(_fence_event);
        _fence_event = nullptr;
    }
    _render_data.reset();
    _failed_import_list.Reset();
    _failed_import_allocator.Reset();
    _failed_import.reset();
    _scene.clear();
    _resources.prune();
    _depth_stencil_buffer.Reset();
    _depth_stencil_heap.Reset();
    _gbuffer_render_pass.release_targets();
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
        RoUninitialize();
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
    auto* renderer = reinterpret_cast<RENDERER*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    const bool ishandled = renderer && renderer->_ui.process_message(window, message, wparam, lparam);
    INPUT_MANAGER::get_instance().process_message(message, wparam,
        renderer && renderer->_ui.wants_mouse());
    if (message == WM_GETMINMAXINFO)
    {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize = { 920, 640 };
        return 0;
    }
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    if (ishandled) return 1;
    return DefWindowProcW(window, message, wparam, lparam);
}
