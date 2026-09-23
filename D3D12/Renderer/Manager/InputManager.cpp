#include "../../Common/stdafx.h"
#include "InputManager.h"

INPUT_MANAGER& INPUT_MANAGER::get_instance()
{
    static INPUT_MANAGER instance;
    return instance;
}

void INPUT_MANAGER::initialize(HWND window)
{
    _window = window;
}

void INPUT_MANAGER::update()
{
    _mouse_delta = {};
    if (!_is_right_mouse_down || _window == nullptr)
    {
        return;
    }

    RECT client_rect{};
    GetClientRect(_window, &client_rect);
    POINT center
    {
        (client_rect.right - client_rect.left) / 2,
        (client_rect.bottom - client_rect.top) / 2
    };
    ClientToScreen(_window, &center);

    POINT cursor_position{};
    GetCursorPos(&cursor_position);
    _mouse_delta.x = cursor_position.x - center.x;
    _mouse_delta.y = cursor_position.y - center.y;

    if (_mouse_delta.x != 0 || _mouse_delta.y != 0)
    {
        SetCursorPos(center.x, center.y);
    }
}

void INPUT_MANAGER::process_message(UINT message, WPARAM wparam)
{
    if (message == WM_RBUTTONDOWN)
    {
        _is_right_mouse_down = true;
        SetCapture(_window);
        center_cursor();
    }
    else if (message == WM_RBUTTONUP)
    {
        _is_right_mouse_down = false;
        ReleaseCapture();
        _mouse_delta = {};
    }
}

bool INPUT_MANAGER::is_key_down(int key) const
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool INPUT_MANAGER::is_right_mouse_down() const
{
    return _is_right_mouse_down;
}

POINT INPUT_MANAGER::get_mouse_delta() const
{
    return _mouse_delta;
}

void INPUT_MANAGER::center_cursor()
{
    if (_window == nullptr)
    {
        return;
    }

    RECT client_rect{};
    GetClientRect(_window, &client_rect);
    POINT center
    {
        (client_rect.right - client_rect.left) / 2,
        (client_rect.bottom - client_rect.top) / 2
    };
    ClientToScreen(_window, &center);
    SetCursorPos(center.x, center.y);
}
