#pragma once

#include "../Common/stdafx.h"

class WINDOW
{
public:
    ~WINDOW();
    bool initialize(HINSTANCE instance, int show_command);
    bool process_messages();
    HWND get_window() const;

private:
    static constexpr wchar_t window_class_name[] = L"D3D12 Engine Renderer";
    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    bool register_window_class();

    HINSTANCE _instance = nullptr;
    HWND _window = nullptr;
};
