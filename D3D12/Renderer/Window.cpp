#include "../Common/stdafx.h"
#include "Window.h"

constexpr wchar_t WINDOW::window_class_name[];

WINDOW::~WINDOW()
{
    if (_window != nullptr) DestroyWindow(_window);
    if (_instance != nullptr) UnregisterClassW(window_class_name, _instance);
}

bool WINDOW::initialize(HINSTANCE instance, int show_command)
{
    _instance = instance;
    if (!register_window_class()) return false;

    RECT client_rect{ 0, 0, 1280, 720 };
    AdjustWindowRect(&client_rect, WS_OVERLAPPEDWINDOW, FALSE);
    _window = CreateWindowExW(0, window_class_name, L"D3D12 Renderer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top, nullptr, nullptr, _instance, this);
    if (_window == nullptr) return false;

    ShowWindow(_window, show_command);
    UpdateWindow(_window);
    return true;
}

bool WINDOW::process_messages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0)
    {
        if (message.message == WM_QUIT) return false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

HWND WINDOW::get_window() const
{
    return _window;
}

bool WINDOW::register_window_class()
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.hInstance = _instance;
    window_class.lpfnWndProc = window_procedure;
    window_class.lpszClassName = window_class_name;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    return RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK WINDOW::window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return TRUE;
    }
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
