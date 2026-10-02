#pragma once

#include "../../Common/stdafx.h"

class SHADER
{
public:
    static SHADER& get_instance();

    SHADER(const SHADER&) = delete;
    SHADER& operator=(const SHADER&) = delete;

    bool compile_shader(
        const wchar_t* file_name,
        const char* entry_point,
        const char* target,
        ComPtr<ID3DBlob>& shader);

private:
    SHADER() = default;
};
