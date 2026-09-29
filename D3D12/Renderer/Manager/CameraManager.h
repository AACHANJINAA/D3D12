#pragma once

#include "../../Common/stdafx.h"

class CAMERA_MANAGER
{
public:
    static CAMERA_MANAGER& get_instance();

    CAMERA_MANAGER(const CAMERA_MANAGER&) = delete;
    CAMERA_MANAGER& operator=(const CAMERA_MANAGER&) = delete;

    void initialize();
    void update(float delta_time);
    void set_orbit_target(const MATH::VECTOR3& target) { _orbit_target = target; }
    MATH::MATRIX4X4 get_view_projection(float aspect_ratio) const;
    const MATH::VECTOR3& get_position() const;

private:
    CAMERA_MANAGER() = default;

    MATH::VECTOR3 _position{ 0.0f, 0.0f, -6.0f };
    MATH::VECTOR3 _orbit_target{};
    float _yaw = 0.0f;
    float _pitch = 0.0f;
    float _orbit_angle = MATH::pi;
    float _orbit_elevation = 0.0f;
    float _orbit_radius = 4.0f;
    bool _is_orbiting = false;
    bool _was_orbit_key_down = false;
};
