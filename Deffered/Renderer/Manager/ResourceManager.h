#pragma once
#include "../../Resource/ModelResource.h"

class RESOURCE_MANAGER
{
public:
    std::shared_ptr<MODEL_RESOURCE> find_model(const std::filesystem::path& path) const
    {
        return _models.find(resource_key(path));
    }
    void publish_model(const std::filesystem::path& path, const std::shared_ptr<MODEL_RESOURCE>& model)
    {
        _models.insert(resource_key(path), model);
    }
    RESOURCE_CACHE<IMAGE_TEXTURE>& get_texture_cache() { return _textures; }
    void prune() { _models.prune(); _textures.prune(); }
    size_t get_model_count() const { return _models.live_count(); }
    size_t get_texture_count() const { return _textures.live_count(); }
private:
    RESOURCE_CACHE<MODEL_RESOURCE> _models;
    RESOURCE_CACHE<IMAGE_TEXTURE> _textures;
};
