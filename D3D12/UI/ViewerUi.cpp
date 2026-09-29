#include "../Common/stdafx.h"
#include "ViewerUi.h"
#include "UiTheme.h"
#include "../ThirdParty/ImGui/imgui.h"
#include "../ThirdParty/ImGui/backends/imgui_impl_win32.h"
#include "../ThirdParty/ImGui/backends/imgui_impl_dx12.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool VIEWER_UI::initialize(HWND window, ID3D12Device* device,
    ID3D12CommandQueue* queue, int frame_count)
{
    IMGUI_CHECKVERSION();
    _context = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().Fonts->AddFontDefault();
    apply_viewer_theme();
    CD3DX12_DESCRIPTOR_HEAP_DESC heap(6, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&_heap))))
    {
        shutdown();
        return false;
    }
    _is_win32_ready = ImGui_ImplWin32_Init(window);
    if (!_is_win32_ready) { shutdown(); return false; }
    ImGui_ImplDX12_InitInfo info{};
    info.Device = device;
    info.CommandQueue = queue;
    info.NumFramesInFlight = frame_count;
    info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    info.SrvDescriptorHeap = _heap.Get();
    // Version 1.91.9b allocates one font descriptor; slots 1..5 are material previews.
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* data,
        D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        *cpu = data->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        *gpu = data->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*,
        D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};
    _is_dx12_ready = ImGui_ImplDX12_Init(&info);
    if (!_is_dx12_ready || !ImGui_ImplDX12_CreateDeviceObjects())
    {
        shutdown();
        return false;
    }
    return true;
}

void VIEWER_UI::set_textures(ID3D12Device* device,
    const std::array<ID3D12Resource*, 5>& textures)
{
    const UINT stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = _heap->GetCPUDescriptorHandleForHeapStart();
    auto gpu = _heap->GetGPUDescriptorHandleForHeapStart();
    for (size_t index = 0; index < textures.size(); ++index)
    {
        cpu.ptr += stride;
        gpu.ptr += stride;
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        // Preview source texels directly; the UI pass has no output gamma conversion.
        view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(textures[index], &view, cpu);
        _textures[index] = gpu.ptr;
    }
}

void VIEWER_UI::begin_frame(VIEWER_SETTINGS& settings, const VIEWER_STATS& stats)
{
    if (_is_frame_open) ImGui::EndFrame();
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    _is_frame_open = true;
    _panels.draw(settings, stats, _textures);
}

void VIEWER_UI::render(ID3D12GraphicsCommandList* list)
{
    ImGui::Render();
    _is_frame_open = false;
    ID3D12DescriptorHeap* heaps[] = { _heap.Get() };
    list->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
}

bool VIEWER_UI::process_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (!_context || !_is_win32_ready) return false;
    return ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam) != 0;
}

bool VIEWER_UI::wants_mouse() const
{
    return _context && ImGui::GetIO().WantCaptureMouse;
}

bool VIEWER_UI::wants_keyboard() const
{
    return _context && ImGui::GetIO().WantCaptureKeyboard;
}

void VIEWER_UI::shutdown()
{
    if (!_context) return;
    if (_is_frame_open) ImGui::EndFrame();
    if (_is_dx12_ready) ImGui_ImplDX12_Shutdown();
    if (_is_win32_ready) ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(_context);
    _context = nullptr;
    _is_dx12_ready = _is_win32_ready = _is_frame_open = false;
    _heap.Reset();
}
