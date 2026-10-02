#include "../Common/stdafx.h"
#include "ModelFileDialog.h"
#include <commdlg.h>

std::filesystem::path MODEL_FILE_DIALOG::open(HWND owner)
{
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"glTF 2.0 (*.gltf)\0*.gltf\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = L"Open Model";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) return path.data();
    const DWORD error = CommDlgExtendedError();
    if (error) show_error(owner, "File dialog failed. Error: " + std::to_string(error));
    return {};
}

void MODEL_FILE_DIALOG::show_error(HWND owner, const std::string& error)
{
    const int count = MultiByteToWideChar(CP_UTF8, 0, error.data(), static_cast<int>(error.size()), nullptr, 0);
    std::wstring message(static_cast<size_t>(count), L'\0');
    if (count) MultiByteToWideChar(CP_UTF8, 0, error.data(), static_cast<int>(error.size()), message.data(), count);
    MessageBoxW(owner, message.c_str(), L"Model Import", MB_OK | MB_ICONERROR);
}
