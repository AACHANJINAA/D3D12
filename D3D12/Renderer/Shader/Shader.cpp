#include "../../Common/stdafx.h"
#include "Shader.h"

namespace
{
    std::filesystem::path get_shader_path(const wchar_t* file_name)
    {
        wchar_t module_path[MAX_PATH]{};
        const DWORD path_length = GetModuleFileNameW(nullptr, module_path, _countof(module_path));
        if (path_length == 0)
        {
            return {};
        }

        return std::filesystem::path(std::wstring(module_path, path_length)).parent_path() /
            L"Renderer" / L"Shader" / file_name;
    }
}

SHADER& SHADER::get_instance()
{
    static SHADER instance;
    return instance;
}

bool SHADER::compile_shader(
    const wchar_t* file_name,
    const char* entry_point,
    const char* target,
    ComPtr<ID3DBlob>& shader)
{
    const std::filesystem::path shader_path = get_shader_path(file_name);
    if (shader_path.empty())
    {
        return false;
    }

    ComPtr<ID3DBlob> shader_error;
    UINT compile_flags = 0;
#if defined(_DEBUG)
    compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    const HRESULT result = D3DCompileFromFile(
        shader_path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point,
        target,
        compile_flags,
        0,
        &shader,
        &shader_error);

    if (FAILED(result) && shader_error != nullptr)
    {
        OutputDebugStringA(static_cast<const char*>(shader_error->GetBufferPointer()));
    }

    return SUCCEEDED(result);
}
