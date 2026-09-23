#pragma once

#include "../../Common/stdafx.h"

class INPUT_MANAGER
{
public:
    static INPUT_MANAGER& get_instance();

    INPUT_MANAGER(const INPUT_MANAGER&) = delete;
    INPUT_MANAGER& operator=(const INPUT_MANAGER&) = delete;

    void initialize(HWND window);
    void update();
    void process_message(UINT message, WPARAM wparam);
    bool is_key_down(int key) const;
    bool is_right_mouse_down() const;
    POINT get_mouse_delta() const;

private:
    INPUT_MANAGER() = default;
    void center_cursor();

    HWND _window = nullptr;
    POINT _mouse_delta{};
    bool _is_right_mouse_down = false;
};
