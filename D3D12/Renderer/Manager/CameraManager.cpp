#include "../../Common/stdafx.h"
#include "CameraManager.h"
#include "InputManager.h"

CAMERA_MANAGER& CAMERA_MANAGER::get_instance()
{
    static CAMERA_MANAGER instance;
    return instance;
}

void CAMERA_MANAGER::initialize()
{
    _position = { 0.0f, 0.0f, -6.0f };
    _yaw = 0.0f;
    _pitch = 0.0f;
    _orbit_angle = MATH::pi;
    _orbit_elevation = 0.0f;
    _orbit_radius = 4.0f;
    _is_orbiting = false;
    _was_orbit_key_down = false;
}

void CAMERA_MANAGER::update(float delta_time)
{
    INPUT_MANAGER& input = INPUT_MANAGER::get_instance();
    constexpr float move_speed = 4.0f;
    constexpr float mouse_sensitivity = 0.005f;
    constexpr float orbit_speed = 0.8f;
    constexpr float orbit_zoom_speed = 3.0f;
    constexpr float min_orbit_radius = 1.5f;
    constexpr float max_orbit_radius = 12.0f;
    constexpr float orbit_elevation_speed = 1.5f;
    constexpr float max_orbit_elevation = MATH::pi / 4.0f;

    const bool is_orbit_key_down = input.is_key_down('R');
    if (is_orbit_key_down && !_was_orbit_key_down)
    {
        _is_orbiting = !_is_orbiting;
    }
    _was_orbit_key_down = is_orbit_key_down;

    if (_is_orbiting)
    {
        _orbit_angle += orbit_speed * delta_time;
        if (input.is_key_down('W'))
        {
            _orbit_radius -= orbit_zoom_speed * delta_time;
        }
        if (input.is_key_down('S'))
        {
            _orbit_radius += orbit_zoom_speed * delta_time;
        }
        _orbit_radius = (std::clamp)(
            _orbit_radius, min_orbit_radius, max_orbit_radius);

        if (input.is_key_down('E'))
        {
            _orbit_elevation += orbit_elevation_speed * delta_time;
        }
        if (input.is_key_down('Q'))
        {
            _orbit_elevation -= orbit_elevation_speed * delta_time;
        }
        _orbit_elevation = (std::clamp)(
            _orbit_elevation, -max_orbit_elevation, max_orbit_elevation);

        const float horizontal_radius = std::cos(_orbit_elevation) * _orbit_radius;
        _position =
        {
            std::sin(_orbit_angle) * horizontal_radius,
            std::sin(_orbit_elevation) * _orbit_radius,
            std::cos(_orbit_angle) * horizontal_radius
        };

        const MATH::VECTOR3 to_target = MATH::subtract({ 0.0f, 0.0f, 0.0f }, _position);
        const MATH::VECTOR3 direction = MATH::normalize(to_target);
        _yaw = std::atan2(direction.x, direction.z);
        _pitch = std::asin(direction.y);
        return;
    }

    if (input.is_right_mouse_down())
    {
        const POINT mouse_delta = input.get_mouse_delta();
        _yaw += static_cast<float>(mouse_delta.x) * mouse_sensitivity;
        _pitch -= static_cast<float>(mouse_delta.y) * mouse_sensitivity;
        _pitch = (std::clamp)(_pitch, -1.5f, 1.5f);
    }

    const MATH::VECTOR3 forward
    {
        std::sin(_yaw) * std::cos(_pitch),
        std::sin(_pitch),
        std::cos(_yaw) * std::cos(_pitch)
    };
    const MATH::VECTOR3 horizontal_forward = MATH::normalize({ forward.x, 0.0f, forward.z });
    const MATH::VECTOR3 right = MATH::normalize(MATH::cross({ 0.0f, 1.0f, 0.0f }, horizontal_forward));
    MATH::VECTOR3 movement{};

    if (input.is_key_down('W')) movement = MATH::add(movement, horizontal_forward);
    if (input.is_key_down('S')) movement = MATH::subtract(movement, horizontal_forward);
    if (input.is_key_down('D')) movement = MATH::add(movement, right);
    if (input.is_key_down('A')) movement = MATH::subtract(movement, right);
    if (input.is_key_down('E')) movement.y += 1.0f;
    if (input.is_key_down('Q')) movement.y -= 1.0f;

    if (MATH::length(movement) > 0.0f)
    {
        _position = MATH::add(_position,
            MATH::multiply(MATH::normalize(movement), move_speed * delta_time));
    }
}

MATH::MATRIX4X4 CAMERA_MANAGER::get_view_projection(float aspect_ratio) const
{
    const MATH::VECTOR3 forward
    {
        std::sin(_yaw) * std::cos(_pitch),
        std::sin(_pitch),
        std::cos(_yaw) * std::cos(_pitch)
    };
    const MATH::MATRIX4X4 view = MATH::matrix_look_at_left_handed(
        _position, MATH::add(_position, forward), { 0.0f, 1.0f, 0.0f });
    const MATH::MATRIX4X4 projection = MATH::matrix_perspective_left_handed(
        MATH::pi / 3.0f, aspect_ratio, 0.1f, 100.0f);
    return MATH::multiply(view, projection);
}

const MATH::VECTOR3& CAMERA_MANAGER::get_position() const
{
    return _position;
}
