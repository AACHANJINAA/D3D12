#include "../Common/stdafx.h"
#include "ModelResource.h"

bool MODEL_RESOURCE::load(ID3D12Device* device, ID3D12GraphicsCommandList* list,
    const std::filesystem::path& path, RESOURCE_CACHE<IMAGE_TEXTURE>& cache, std::string& error)
{
    if (!_mesh.load(path)) { error = _mesh.get_error(); return false; }
    _geometry = std::make_shared<MESH_RESOURCE>();
    if (!_geometry->initialize(device, list, _mesh))
    {
        error = "Could not create model geometry buffers.";
        return false;
    }
    for (const auto& parameters : _mesh.get_materials())
    {
        auto material = std::make_shared<MATERIAL_RESOURCE>();
        if (!material->initialize(device, list, parameters, cache))
        {
            error = "Could not decode or upload textures for material: " + parameters.name;
            return false;
        }
        _materials.push_back(std::move(material));
    }
    const auto name = path.filename().u8string();
    _name.assign(reinterpret_cast<const char*>(name.data()), name.size());
    return true;
}
