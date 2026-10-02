#pragma once
#include "../Resource/ModelResource.h"

struct MATERIAL_SETTINGS
{
    MATH::VECTOR4 base_color{ 1, 1, 1, 1 };
    float metallic_multiplier = 1;
    float roughness_multiplier = 1;
    float emissive_multiplier = 1;
};

struct SCENE_OBJECT
{
    UINT64 id = 0;
    std::string name;
    std::shared_ptr<MODEL_RESOURCE> model;
    bool isvisible = true;
    MATH::VECTOR3 position{};
    MATH::VECTOR3 rotation{};
    MATH::VECTOR3 scale{ 1, 1, 1 };
    size_t material_index = 0;
    std::vector<MATERIAL_SETTINGS> materials;

    MATH::MATRIX4X4 get_transform() const;
    MATH::VECTOR3 get_center() const;
    float get_radius() const;
};

class SCENE
{
public:
    static constexpr size_t object_limit = 128;
    UINT64 add(const std::shared_ptr<MODEL_RESOURCE>& model);
    bool replace(UINT64 id, const std::shared_ptr<MODEL_RESOURCE>& model);
    bool erase(UINT64 id);
    void clear() { _objects.clear(); _selected_id = 0; }
    void select(UINT64 id) { if (id == 0 || find(id)) _selected_id = id; }
    UINT64 get_selected_id() const { return _selected_id; }
    SCENE_OBJECT* find(UINT64 id);
    SCENE_OBJECT* selected() { return find(_selected_id); }
    const std::vector<SCENE_OBJECT>& get_objects() const { return _objects; }
    std::vector<SCENE_OBJECT>& get_objects() { return _objects; }
    size_t visible_count() const;
    size_t triangle_count() const;
private:
    UINT64 _next_id = 1;
    UINT64 _selected_id = 0;
    std::vector<SCENE_OBJECT> _objects;
};
