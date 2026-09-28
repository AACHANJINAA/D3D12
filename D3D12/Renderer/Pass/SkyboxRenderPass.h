#pragma once

#include "../../Common/stdafx.h"

class SKYBOX_RENDER_PASS
{
public:
    bool initialize(ID3D12Device* device);
    void render(ID3D12GraphicsCommandList* command_list) const;
};
