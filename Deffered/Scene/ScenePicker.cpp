#include "../Common/stdafx.h"
#include "ScenePicker.h"
#include <limits>

namespace
{
    using namespace MATH;

    VECTOR3 inverse_direction(const VECTOR3& value, const MATRIX4X4& normal)
    {
        // The inverse linear transform is the transpose of the normal matrix.
        return {
            value.x * normal.values[0][0] + value.y * normal.values[0][1] + value.z * normal.values[0][2],
            value.x * normal.values[1][0] + value.y * normal.values[1][1] + value.z * normal.values[1][2],
            value.x * normal.values[2][0] + value.y * normal.values[2][1] + value.z * normal.values[2][2] };
    }

    bool intersects_bounds(const VECTOR3& origin, const VECTOR3& direction,
        const VECTOR3& minimum, const VECTOR3& maximum, float distance)
    {
        float near_distance = 0;
        const float origins[] = { origin.x, origin.y, origin.z };
        const float directions[] = { direction.x, direction.y, direction.z };
        const float lower[] = { minimum.x, minimum.y, minimum.z };
        const float upper[] = { maximum.x, maximum.y, maximum.z };
        for (int axis = 0; axis < 3; ++axis)
        {
            if (directions[axis] == 0)
            {
                if (origins[axis] < lower[axis] || origins[axis] > upper[axis]) return false;
                continue;
            }
            float first = (lower[axis] - origins[axis]) / directions[axis];
            float last = (upper[axis] - origins[axis]) / directions[axis];
            if (first > last) std::swap(first, last);
            near_distance = (std::max)(near_distance, first);
            distance = (std::min)(distance, last);
            if (near_distance > distance) return false;
        }
        return true;
    }

    bool intersects_triangle(const VECTOR3& origin, const VECTOR3& direction,
        const VECTOR3& a, const VECTOR3& b, const VECTOR3& c, float& distance)
    {
        const auto edge1 = subtract(b, a), edge2 = subtract(c, a);
        const auto perpendicular = cross(direction, edge2);
        const float determinant = dot(edge1, perpendicular);
        const float tolerance = 1e-7f * length(edge1) * length(edge2) * length(direction);
        if (std::abs(determinant) <= tolerance) return false;
        const auto offset = subtract(origin, a);
        const float u = dot(offset, perpendicular) / determinant;
        if (u < 0 || u > 1) return false;
        const auto q = cross(offset, edge1);
        const float v = dot(direction, q) / determinant;
        if (v < 0 || u + v > 1) return false;
        const float hit = dot(edge2, q) / determinant;
        if (!std::isfinite(hit) || hit < 0 || hit >= distance) return false;
        distance = hit;
        return true;
    }
}

bool SCENE_PICKER::pick_mesh(const GLTF_MESH& mesh, const MATH::MATRIX4X4& world,
    const MATH::VECTOR3& origin, const MATH::VECTOR3& direction, float& distance)
{
    bool ishit = false;
    const auto& vertices = mesh.get_vertices();
    const auto& indices = mesh.get_indices();
    for (const auto& primitive : mesh.get_primitives())
    {
        const auto transform = MATH::multiply(primitive.node_transform, world);
        MATH::MATRIX4X4 normal;
        float sign;
        if (!MATH::normal_matrix(transform, normal, sign)) continue;
        const MATH::VECTOR3 translation{ transform.values[3][0], transform.values[3][1], transform.values[3][2] };
        const auto local_origin = inverse_direction(MATH::subtract(origin, translation), normal);
        // Do not normalize: preserve the same ray parameter across differently scaled objects.
        const auto local_direction = inverse_direction(direction, normal);
        if (!intersects_bounds(local_origin, local_direction,
            primitive.local_minimum, primitive.local_maximum, distance)) continue;
        for (size_t index = primitive.first_index; index < size_t(primitive.first_index) + primitive.index_count; index += 3)
        {
            const auto vertex = [&](size_t offset)
            {
                const auto& value = vertices[indices[offset]].position;
                return MATH::VECTOR3{ value[0], value[1], value[2] };
            };
            if (intersects_triangle(local_origin, local_direction,
                vertex(index), vertex(index + 1), vertex(index + 2), distance)) ishit = true;
        }
    }
    return ishit;
}

UINT64 SCENE_PICKER::pick(const SCENE& scene, const MATH::VECTOR3& origin, const MATH::VECTOR3& direction)
{
    UINT64 selected = 0;
    float distance = (std::numeric_limits<float>::max)();
    for (const auto& object : scene.get_objects())
        if (object.isvisible && object.model &&
            pick_mesh(object.model->get_mesh(), object.get_transform(), origin, direction, distance))
            selected = object.id;
    return selected;
}
