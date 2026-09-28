#pragma once

#include "../Common/stdafx.h"

struct GLTF_VERTEX
{
    float position[3];
    float normal[3];
    float tangent[3];
    float uv[2];
};

class GLTF_MESH
{
public:
    bool load(const std::filesystem::path& file_path);

    const std::vector<GLTF_VERTEX>& get_vertices() const;
    const std::vector<uint32_t>& get_indices() const;

private:
    std::vector<GLTF_VERTEX> _vertices;
    std::vector<uint32_t> _indices;
};
