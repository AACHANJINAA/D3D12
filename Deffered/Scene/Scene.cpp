#include "../Common/stdafx.h"
#include "Scene.h"
#include <stdexcept>

namespace
{
    void validate_draw_count(const SCENE& scene, const MODEL_RESOURCE& model, UINT64 replaced_id)
    {
        size_t count = model.get_mesh().get_primitives().size();
        for (const auto& object : scene.get_objects())
            if (object.id != replaced_id) count += object.model->get_mesh().get_primitives().size();
        if (count > 65536) throw std::runtime_error("Scene draw limit reached.");
    }
}

MATH::MATRIX4X4 SCENE_OBJECT::get_transform() const { return MATH::matrix_world(position, rotation, scale); }
MATH::VECTOR3 SCENE_OBJECT::get_center() const
{
    return MATH::transform_point(model->get_mesh().get_center(), get_transform());
}
float SCENE_OBJECT::get_radius() const
{
    return model->get_mesh().get_radius() * (std::max)({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
}

SCENE_OBJECT* SCENE::find(UINT64 id)
{
    for (auto& object : _objects) if (object.id == id) return &object;
    return nullptr;
}

UINT64 SCENE::add(const std::shared_ptr<MODEL_RESOURCE>& model)
{
    if (!model || _objects.size() >= object_limit) throw std::runtime_error("Scene object limit reached.");
    validate_draw_count(*this, *model, 0);
    SCENE_OBJECT object;
    object.id = _next_id;
    object.model = model;
    object.name = model->get_name() + " [" + std::to_string(object.id) + "]";
    object.materials.resize(model->get_materials().size());
    if (auto* previous = selected())
    {
        const auto center = previous->get_center();
        const float spacing = (previous->get_radius() + model->get_mesh().get_radius()) * 1.2f;
        object.position = MATH::subtract(MATH::add(center, { spacing, 0, 0 }), model->get_mesh().get_center());
    }
    _objects.push_back(std::move(object));
    _selected_id = _next_id++;
    return _selected_id;
}

bool SCENE::replace(UINT64 id, const std::shared_ptr<MODEL_RESOURCE>& model)
{
    auto* object = find(id);
    if (!object || !model) return false;
    validate_draw_count(*this, *model, id);
    // Prepare allocations before releasing the old resource or changing the live object.
    auto name = model->get_name() + " [" + std::to_string(id) + "]";
    std::vector<MATERIAL_SETTINGS> materials(model->get_materials().size());
    object->model = model;
    object->name = std::move(name);
    object->materials = std::move(materials);
    object->material_index = 0;
    object->isvisible = true;
    _selected_id = id;
    return true;
}

bool SCENE::erase(UINT64 id)
{
    const auto found = std::find_if(_objects.begin(), _objects.end(),
        [id](const auto& object) { return object.id == id; });
    if (found == _objects.end()) return false;
    const auto index = static_cast<size_t>(found - _objects.begin());
    _objects.erase(found);
    if (_selected_id == id)
        _selected_id = _objects.empty() ? 0 : _objects[(std::min)(index, _objects.size() - 1)].id;
    return true;
}

size_t SCENE::visible_count() const
{
    return std::count_if(_objects.begin(), _objects.end(), [](const auto& object) { return object.isvisible; });
}
size_t SCENE::triangle_count() const
{
    size_t count = 0;
    for (const auto& object : _objects) if (object.isvisible) count += object.model->get_mesh().get_triangle_count();
    return count;
}
