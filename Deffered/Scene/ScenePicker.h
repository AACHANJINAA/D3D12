#pragma once
#include "Scene.h"

class SCENE_PICKER
{
public:
    static UINT64 pick(const SCENE& scene, const MATH::VECTOR3& origin, const MATH::VECTOR3& direction);
    static bool pick_mesh(const GLTF_MESH& mesh, const MATH::MATRIX4X4& world,
        const MATH::VECTOR3& origin, const MATH::VECTOR3& direction, float& distance);
};
