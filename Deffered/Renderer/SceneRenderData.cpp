#include "../Common/stdafx.h"
#include "SceneRenderData.h"
#include <unordered_map>

namespace
{
    struct FRAME_DATA
    {
        MATH::MATRIX4X4 transform;
        MATH::VECTOR3 light_direction;
        float light_intensity;
        MATH::VECTOR3 light_color;
        float exposure;
        MATH::VECTOR3 camera_position;
        float environment_intensity;
        MATH::VECTOR4 base_color_factor;
        float metallic_factor;
        float roughness_factor;
        float ambient_strength;
        float depth_range;
        MATH::VECTOR3 emissive_factor;
        UINT view_mode;
        MATH::MATRIX4X4 world_transform;
        MATH::MATRIX4X4 normal_transform;
        float normal_scale;
        float occlusion_strength;
        float tangent_sign;
        float selection_mask;
    };
    static_assert(sizeof(FRAME_DATA) == 304);
    constexpr size_t stride = (sizeof(FRAME_DATA) + 255u) & ~255u;
    struct INSTANCE_DATA
    {
        MATH::MATRIX4X4 world, normal;
        float tangent_sign = 1, selection = 0;
        float padding[2]{};
    };
    static_assert(sizeof(INSTANCE_DATA) == 144);
    struct GROUP
    {
        GBUFFER_DRAW draw;
        FRAME_DATA constants;
        std::vector<INSTANCE_DATA> instances;
    };
}

bool SCENE_RENDER_DATA::reserve(ID3D12Device* device, size_t bytes)
{
    if (_capacity >= bytes) return true;
    constexpr size_t maximum = 65537 * stride + 65536 * sizeof(INSTANCE_DATA);
    if (bytes > maximum) return false;
    const size_t capacity = (std::min)(maximum, (std::max)(bytes, _capacity * 2));
    const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto description = CD3DX12_RESOURCE_DESC::buffer(capacity);
    ComPtr<ID3D12Resource> buffer;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)))) return false;
    const D3D12_RANGE read{0, 0};
    UINT8* mapped = nullptr;
    if (FAILED(buffer->Map(0, &read, reinterpret_cast<void**>(&mapped)))) return false;
    reset();
    _buffer = std::move(buffer);
    _mapped = mapped;
    _capacity = capacity;
    return true;
}

bool SCENE_RENDER_DATA::update(ID3D12Device* device, const SCENE& scene, const VIEWER_SETTINGS& settings,
    const MATH::MATRIX4X4& view_projection, const MATH::VECTOR3& camera, bool isinstancing)
{
    size_t count = 0;
    for (const auto& object : scene.get_objects())
        if (object.isvisible) count += object.model->get_mesh().get_primitives().size();
    if (count > 65536) return false;
    std::vector<GROUP> groups;
    std::unordered_map<std::string, size_t> lookup;
    FRAME_DATA common{};
    common.transform = view_projection;
    common.light_direction = settings.light_direction;
    common.light_intensity = settings.light_intensity;
    common.light_color = settings.light_color;
    common.exposure = settings.exposure;
    common.camera_position = camera;
    common.environment_intensity = settings.environment_intensity;
    common.ambient_strength = 0.03f;
    common.depth_range = settings.depth_range;
    common.view_mode = static_cast<UINT>(settings.mode);
    for (const auto& object : scene.get_objects())
    {
        if (!object.isvisible) continue;
        const auto& model = *object.model;
        const auto& primitives = model.get_mesh().get_primitives();
        for (size_t primitive_index = 0; primitive_index < primitives.size(); ++primitive_index)
        {
            const auto& primitive = primitives[primitive_index];
            FRAME_DATA data = common;
            const auto& material = *model.get_materials().at(primitive.material_index);
            const auto& parameters = material.get_parameters();
            const auto& values = object.materials.at(primitive.material_index);
            INSTANCE_DATA instance;
            instance.world = MATH::multiply(primitive.node_transform, object.get_transform());
            if (!MATH::normal_matrix(instance.world, instance.normal, instance.tangent_sign)) return false;
            instance.selection = object.id == scene.get_selected_id() ? 1.0f : 0.0f;
            data.base_color_factor = { parameters.base_color_factor[0] * values.base_color.x,
                parameters.base_color_factor[1] * values.base_color.y,
                parameters.base_color_factor[2] * values.base_color.z,
                parameters.base_color_factor[3] * values.base_color.w };
            data.metallic_factor = parameters.metallic_factor * values.metallic_multiplier;
            data.roughness_factor = parameters.roughness_factor * values.roughness_multiplier;
            data.emissive_factor = { parameters.emissive_factor[0] * values.emissive_multiplier,
                parameters.emissive_factor[1] * values.emissive_multiplier,
                parameters.emissive_factor[2] * values.emissive_multiplier };
            data.normal_scale = parameters.normal_scale;
            data.occlusion_strength = parameters.occlusion_strength;
            size_t group_index = groups.size();
            if (isinstancing)
            {
                // Instance transforms and selection stay out of the material batch key.
                const auto pointer = reinterpret_cast<uintptr_t>(&model);
                std::string key(reinterpret_cast<const char*>(&pointer), sizeof(pointer));
                key.append(reinterpret_cast<const char*>(&primitive_index), sizeof(primitive_index));
                key.append(reinterpret_cast<const char*>(&data), sizeof(data));
                group_index = lookup.emplace(std::move(key), groups.size()).first->second;
            }
            if (group_index == groups.size())
                groups.push_back({ { model.get_geometry().get_vertex_view(), model.get_geometry().get_index_view(),
                    material.get_textures().get_srv_heap(), 0, primitive.first_index, primitive.index_count }, data, {} });
            groups[group_index].instances.push_back(instance);
        }
    }
    const size_t instance_offset = (groups.size() + 1) * stride;
    if (!reserve(device, instance_offset + count * sizeof(INSTANCE_DATA))) return false;
    _draws.clear();
    _instance_count = count;
    std::memcpy(_mapped, &common, sizeof(common));
    size_t first_instance = 0;
    const auto address = _buffer->GetGPUVirtualAddress();
    for (size_t index = 0; index < groups.size(); ++index)
    {
        auto& group = groups[index];
        const size_t constants = (index + 1) * stride;
        const size_t instances = instance_offset + first_instance * sizeof(INSTANCE_DATA);
        std::memcpy(_mapped + constants, &group.constants, sizeof(group.constants));
        std::memcpy(_mapped + instances, group.instances.data(), group.instances.size() * sizeof(INSTANCE_DATA));
        group.draw.constants = address + constants;
        group.draw.instances = address + instances;
        group.draw.instance_count = static_cast<UINT>(group.instances.size());
        first_instance += group.instances.size();
        _draws.push_back(group.draw);
    }
    return true;
}

void SCENE_RENDER_DATA::reset()
{
    if (_buffer && _mapped) _buffer->Unmap(0, nullptr);
    _mapped = nullptr;
    _capacity = 0;
    _instance_count = 0;
    _draws.clear();
    _buffer.Reset();
}
