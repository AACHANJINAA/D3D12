#include "../../Common/stdafx.h"
#include "LightManager.h"

LIGHT_MANAGER& LIGHT_MANAGER::get_instance()
{
    static LIGHT_MANAGER instance;
    return instance;
}

void LIGHT_MANAGER::initialize()
{
    _directional_light = {};
    _directional_light.direction = { -0.4f, -0.8f, -0.5f };
    _directional_light.intensity = 1.5f;
    _directional_light.color = { 1.0f, 0.95f, 0.85f };
}

const DIRECTIONAL_LIGHT& LIGHT_MANAGER::get_directional_light() const
{
    return _directional_light;
}
