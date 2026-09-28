#include "../../Common/stdafx.h"
#include "SkyboxRenderPass.h"

bool SKYBOX_RENDER_PASS::initialize(ID3D12Device* device)
{
    return device != nullptr;
}

void SKYBOX_RENDER_PASS::render(ID3D12GraphicsCommandList* command_list) const
{
    // The cubemap loader and skybox PSO will be added in the next step.
    (void)command_list;
}
