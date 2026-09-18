#pragma once

#include <Windows.h>

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
    static constexpr wchar_t window_class_name[] = L"D3D12 Engine Renderer";
    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    bool register_window_class();
    bool create_window(int show_command);
    void shutdown();

    HINSTANCE _instance = nullptr;
    HWND _window = nullptr;
};
