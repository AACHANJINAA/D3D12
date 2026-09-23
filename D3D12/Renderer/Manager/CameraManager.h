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
    MATH::MATRIX4X4 get_view_projection(float aspect_ratio) const;

private:
    CAMERA_MANAGER() = default;

    MATH::VECTOR3 _position{ 0.0f, 0.0f, -6.0f };
    float _yaw = 0.0f;
    float _pitch = 0.0f;
};
