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
    _directional_light.intensity = 9.0f;
    _directional_light.color = { 1.0f, 0.95f, 0.85f };
    _is_orbiting = false;
}

void LIGHT_MANAGER::update(float delta_time)
{
    if (!_is_orbiting)
    {
        return;
    }

    constexpr float orbit_speed = 0.8f;
    const float angle = orbit_speed * delta_time;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float direction_x = _directional_light.direction.x;
    const float direction_z = _directional_light.direction.z;
    _directional_light.direction.x = direction_x * cosine - direction_z * sine;
    _directional_light.direction.z = direction_x * sine + direction_z * cosine;
}

void LIGHT_MANAGER::toggle_orbit()
{
    _is_orbiting = !_is_orbiting;
}

const DIRECTIONAL_LIGHT& LIGHT_MANAGER::get_directional_light() const
{
    return _directional_light;
}
