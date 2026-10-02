#pragma once

#include "../../Common/stdafx.h"

class CAMERA_MANAGER
{
public:
    static CAMERA_MANAGER& get_instance();

    CAMERA_MANAGER(const CAMERA_MANAGER&) = delete;
    CAMERA_MANAGER& operator=(const CAMERA_MANAGER&) = delete;

    void initialize();
    void frame_model(const MATH::VECTOR3& center, float radius, float aspect_ratio);
    void update(float delta_time);
    void set_orbit_target(const MATH::VECTOR3& target) { _orbit_target = target; }
    void set_target_bounds(const MATH::VECTOR3& center, float radius)
    {
        _orbit_target = center;
        _model_radius = (std::max)(radius, 0.0001f);
    }
    void set_scene_distance(float distance) { _scene_distance = distance; }
    MATH::MATRIX4X4 get_view_projection(float aspect_ratio) const;
    MATH::MATRIX4X4 get_view() const;
    MATH::MATRIX4X4 get_projection(float aspect_ratio) const;
    void set_pose(const MATH::VECTOR3& position, const MATH::VECTOR3& target);
    void get_pick_ray(float x, float y, float aspect_ratio,
        MATH::VECTOR3& origin, MATH::VECTOR3& direction) const;
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
    float _model_radius = 1.0f;
    float _scene_distance = 100.0f;
    bool _is_orbiting = false;
    bool _was_orbit_key_down = false;
};
