#pragma once

#include "GltfMesh.h"
#include "MeshResource.h"
#include "MaterialResource.h"

class MODEL_RESOURCE
{
public:
    bool load(ID3D12Device* device, ID3D12GraphicsCommandList* list,
        const std::filesystem::path& path, RESOURCE_CACHE<IMAGE_TEXTURE>& cache, std::string& error);
    const GLTF_MESH& get_mesh() const { return _mesh; }
    const MESH_RESOURCE& get_geometry() const { return *_geometry; }
    const std::vector<std::shared_ptr<MATERIAL_RESOURCE>>& get_materials() const { return _materials; }
    const std::string& get_name() const { return _name; }
    void release_uploads()
    {
        if (_geometry) _geometry->release_uploads();
        for (auto& material : _materials) material->release_uploads();
    }

private:
    GLTF_MESH _mesh;
    std::shared_ptr<MESH_RESOURCE> _geometry;
    std::vector<std::shared_ptr<MATERIAL_RESOURCE>> _materials;
    std::string _name;
};
