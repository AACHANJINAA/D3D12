#pragma once

#include "../Common/stdafx.h"
#include "ViewerPanels.h"
#include "ModelImportDialog.h"

struct ImGuiContext;
struct BENCHMARK_UI_STATE;

class VIEWER_UI
{
public:
    bool initialize(HWND window, ID3D12Device* device, ID3D12CommandQueue* queue,
        int frame_count);
    void set_textures(ID3D12Device* device, const std::array<ID3D12Resource*, 5>& textures);
    void begin_frame(VIEWER_SETTINGS& settings, SCENE& scene, const VIEWER_STATS& stats,
        bool ispanels_visible = true, BENCHMARK_UI_STATE* comparison = nullptr);
    void render(ID3D12GraphicsCommandList* list);
    void shutdown();
    bool process_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    bool wants_mouse() const;
    bool wants_keyboard() const;
    VIEWER_REQUEST take_request();
    void show_import_error(const std::string& error) const;

private:
    HWND _window = nullptr;
    ImGuiContext* _context = nullptr;
    ComPtr<ID3D12DescriptorHeap> _heap;
    VIEWER_PANELS _panels;
    MODEL_IMPORT_DIALOG _import_dialog;
    std::array<UINT64, 5> _textures{};
    bool _is_win32_ready = false;
    bool _is_dx12_ready = false;
    bool _is_frame_open = false;
};
