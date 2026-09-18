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
}
