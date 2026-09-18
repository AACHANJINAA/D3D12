#include "Renderer.h"

#include <cstdlib>

constexpr wchar_t RENDERER::window_class_name[];

RENDERER::~RENDERER()
{
    shutdown();
}

bool RENDERER::initialize(HINSTANCE instance, int show_command)
{
    _instance = instance;

    if (!register_window_class())
    {
        return false;
    }

    return create_window(show_command);
}

int RENDERER::run()
{
    MSG message{};

    while (true)
    {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);

        if (result == -1)
        {
            return EXIT_FAILURE;
        }

        if (result == 0)
        {
            return static_cast<int>(message.wParam);
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
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
    constexpr int window_width = 1280;
    constexpr int window_height = 720;

    RECT client_rect{ 0, 0, window_width, window_height };
    AdjustWindowRect(&client_rect, window_style, FALSE);

    _window = CreateWindowExW(
        0,
        window_class_name,
        L"D3D12 Renderer",
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top,
        nullptr,
        nullptr,
        _instance,
        this);

    if (_window == nullptr)
    {
        return false;
    }

    ShowWindow(_window, show_command);
    UpdateWindow(_window);
    return true;
}

void RENDERER::shutdown()
{
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

    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}
