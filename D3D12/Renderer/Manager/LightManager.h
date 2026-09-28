#pragma once

#include "../../Common/stdafx.h"

struct DIRECTIONAL_LIGHT
{
    MATH::VECTOR3 direction{ -0.4f, -0.8f, -0.5f };
    float intensity = 1.5f;
    MATH::VECTOR3 color{ 1.0f, 0.95f, 0.85f };
    float padding = 0.0f;
};

class LIGHT_MANAGER
{
public:
    static LIGHT_MANAGER& get_instance();

    LIGHT_MANAGER(const LIGHT_MANAGER&) = delete;
    LIGHT_MANAGER& operator=(const LIGHT_MANAGER&) = delete;

    void initialize();
    void update(float delta_time);
    void toggle_orbit();
    const DIRECTIONAL_LIGHT& get_directional_light() const;

private:
    LIGHT_MANAGER() = default;

    DIRECTIONAL_LIGHT _directional_light;
    bool _is_orbiting = false;
};
