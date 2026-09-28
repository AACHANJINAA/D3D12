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
    float base_color_factor[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    float emissive_factor[3] { 0.0f, 0.0f, 0.0f };
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
};

class GLTF_MESH
{
public:
    bool load(const std::filesystem::path& file_path);

    const std::vector<GLTF_VERTEX>& get_vertices() const;
    const std::vector<uint32_t>& get_indices() const;
    const GLTF_MATERIAL& get_material() const;

private:
    std::vector<GLTF_VERTEX> _vertices;
    std::vector<uint32_t> _indices;
    GLTF_MATERIAL _material;
};
