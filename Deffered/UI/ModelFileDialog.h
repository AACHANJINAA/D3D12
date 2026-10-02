#pragma once

#include "../Common/stdafx.h"

class MODEL_FILE_DIALOG
{
public:
    static std::filesystem::path open(HWND owner);
    static void show_error(HWND owner, const std::string& error);
};
