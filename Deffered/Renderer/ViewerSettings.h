#pragma once

#include "../Common/stdafx.h"

enum class VIEW_MODE : UINT
{
    lit, base_color, normal, metallic, roughness, ao, emissive, depth, wireframe
};

struct VIEWER_SETTINGS
{
    VIEW_MODE mode = VIEW_MODE::lit;
    bool is_skybox_visible = true;
    bool is_light_orbiting = false;
    float exposure = 0.0f;
    float environment_intensity = 1.0f;
    float depth_range = 20.0f;
    MATH::VECTOR3 light_direction{ -0.4f, -0.8f, -0.5f };
    MATH::VECTOR3 light_color{ 1.0f, 0.95f, 0.85f };
    float light_intensity = 9.0f;
};
