#pragma once
#include "GltfMesh.h"
#include "Texture.h"

class MATERIAL_RESOURCE
{
public:
    bool initialize(ID3D12Device* device, ID3D12GraphicsCommandList* list,
        const GLTF_MATERIAL& material, RESOURCE_CACHE<IMAGE_TEXTURE>& cache)
    {
        _material = material;
        return _textures.initialize(device, list, material.texture_paths, &cache);
    }
    const GLTF_MATERIAL& get_parameters() const { return _material; }
    const TEXTURE_SET& get_textures() const { return _textures; }
    void release_uploads() { _textures.release_uploads(); }
private:
    GLTF_MATERIAL _material;
    TEXTURE_SET _textures;
};
