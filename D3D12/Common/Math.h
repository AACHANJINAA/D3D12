#pragma once

#include <cmath>

namespace MATH
{
    struct VECTOR2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct VECTOR3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct VECTOR4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    struct MATRIX4X4
    {
        float values[4][4]{};
    };

    constexpr float pi = 3.14159265358979323846f;

    constexpr VECTOR3 add(const VECTOR3& left, const VECTOR3& right)
    {
        return { left.x + right.x, left.y + right.y, left.z + right.z };
    }

    constexpr VECTOR3 subtract(const VECTOR3& left, const VECTOR3& right)
    {
        return { left.x - right.x, left.y - right.y, left.z - right.z };
    }

    constexpr VECTOR3 multiply(const VECTOR3& value, float scalar)
    {
        return { value.x * scalar, value.y * scalar, value.z * scalar };
    }

    constexpr float dot(const VECTOR3& left, const VECTOR3& right)
    {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    }

    constexpr VECTOR3 cross(const VECTOR3& left, const VECTOR3& right)
    {
        return
        {
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x
        };
    }

    inline float length(const VECTOR3& value)
    {
        return std::sqrt(dot(value, value));
    }

    inline VECTOR3 normalize(const VECTOR3& value)
    {
        const float value_length = length(value);
        if (value_length <= 0.0f)
        {
            return {};
        }
        return multiply(value, 1.0f / value_length);
    }

    constexpr MATRIX4X4 identity_matrix()
    {
        return
        {
            {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 0.0f, 1.0f }
            }
        };
    }

    constexpr MATRIX4X4 multiply(const MATRIX4X4& left, const MATRIX4X4& right)
    {
        MATRIX4X4 result{};
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                for (int index = 0; index < 4; ++index)
                {
                    result.values[row][column] += left.values[row][index] * right.values[index][column];
                }
            }
        }
        return result;
    }

    inline MATRIX4X4 matrix_look_at_left_handed(
        const VECTOR3& eye,
        const VECTOR3& target,
        const VECTOR3& up)
    {
        const VECTOR3 z_axis = normalize(subtract(target, eye));
        const VECTOR3 x_axis = normalize(cross(up, z_axis));
        const VECTOR3 y_axis = cross(z_axis, x_axis);

        MATRIX4X4 result = identity_matrix();
        result.values[0][0] = x_axis.x;
        result.values[0][1] = y_axis.x;
        result.values[0][2] = z_axis.x;
        result.values[1][0] = x_axis.y;
        result.values[1][1] = y_axis.y;
        result.values[1][2] = z_axis.y;
        result.values[2][0] = x_axis.z;
        result.values[2][1] = y_axis.z;
        result.values[2][2] = z_axis.z;
        result.values[3][0] = -dot(x_axis, eye);
        result.values[3][1] = -dot(y_axis, eye);
        result.values[3][2] = -dot(z_axis, eye);
        return result;
    }

    inline MATRIX4X4 matrix_perspective_left_handed(
        float field_of_view,
        float aspect_ratio,
        float near_plane,
        float far_plane)
    {
        const float height = 1.0f / std::tan(field_of_view * 0.5f);
        const float width = height / aspect_ratio;

        MATRIX4X4 result{};
        result.values[0][0] = width;
        result.values[1][1] = height;
        result.values[2][2] = far_plane / (far_plane - near_plane);
        result.values[2][3] = 1.0f;
        result.values[3][2] = -near_plane * far_plane / (far_plane - near_plane);
        return result;
    }
}
