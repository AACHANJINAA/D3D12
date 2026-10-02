#pragma once

#include "../Common/stdafx.h"

struct GLTF_VERTEX
{
    float position[3];
    float normal[3];
    float tangent[4];
    float uv[2];
};

struct GLTF_MATERIAL
{
    std::string name;
    float base_color_factor[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    float emissive_factor[3] { 0.0f, 0.0f, 0.0f };
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    float normal_scale = 1.0f;
    float occlusion_strength = 1.0f;
    std::array<std::filesystem::path, 5> texture_paths;
};

struct GLTF_PRIMITIVE
{
    UINT first_index = 0;
    UINT index_count = 0;
    UINT first_vertex = 0;
    UINT vertex_count = 0;
    UINT material_index = 0;
    MATH::VECTOR3 local_minimum{};
    MATH::VECTOR3 local_maximum{};
    MATH::MATRIX4X4 node_transform = MATH::identity_matrix();
};

class GLTF_MESH
{
public:
    bool load(const std::filesystem::path& file_path);

    const std::vector<GLTF_VERTEX>& get_vertices() const;
    const std::vector<uint32_t>& get_indices() const;
    const std::vector<GLTF_MATERIAL>& get_materials() const { return _materials; }
    const std::vector<GLTF_PRIMITIVE>& get_primitives() const { return _primitives; }
    const GLTF_MATERIAL& get_material() const { return _materials.at(0); }
    size_t get_triangle_count() const;
    const std::string& get_error() const { return _error; }
    const MATH::VECTOR3& get_center() const { return _center; }
    float get_radius() const { return _radius; }

private:
    std::vector<GLTF_VERTEX> _vertices;
    std::vector<uint32_t> _indices;
    std::vector<GLTF_MATERIAL> _materials;
    std::vector<GLTF_PRIMITIVE> _primitives;
    MATH::VECTOR3 _center{};
    float _radius = 1.0f;
    std::string _error;
};
