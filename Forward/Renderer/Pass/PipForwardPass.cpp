#include "../../Common/stdafx.h"
#include "PipForwardPass.h"
#include "../Shader/Shader.h"
#include <cstddef>
#include <unordered_map>
#include <limits>

namespace
{
    using namespace MATH;
    struct CAMERA_DATA { MATRIX4X4 view, projection; VECTOR4 position; };
    struct WORLD_DATA { MATRIX4X4 world, normal; int receive_shadow = 0, other_player = -1; float padding[2]{}; };
    struct LIGHT_DATA
    {
        VECTOR4 ambient{}, diffuse{}, specular{};
        VECTOR3 position{}; float falloff = 1;
        VECTOR3 direction{0,-1,0}; float theta = 0.5f;
        VECTOR3 attenuation{1,0,0}; float phi = 0.8f;
        int isenabled = 0, type = 0; float range = 1000, padding = 0;
    };
    struct LIGHTS_DATA { LIGHT_DATA lights[16]; VECTOR4 ambient; VECTOR4 sh[9]; int count = 1; float padding[3]{}; };
    struct SHADOW_DATA { MATRIX4X4 matrices[6]{}; float near_split = 0, mid_split = 0, bias = 0, max_distance = -(std::numeric_limits<float>::max)(); };
    struct MATERIAL_DATA
    {
        VECTOR4 base_color, emissive_metallic;
        float roughness = 1, normal_scale = 1, alpha_cutoff = 0.5f; int alpha_mode = 0;
        int double_sided = 0, has_base = 0, has_metal = 0, has_normal = 0;
        int has_emissive = 0, has_ao = 0; float specular = 1, padding = 0;
        int uv_channels[4]{};
        float uv_data[20]{};
    };
    static_assert(sizeof(CAMERA_DATA) == 144 && sizeof(WORLD_DATA) == 144);
    static_assert(sizeof(LIGHT_DATA) == 112 && sizeof(LIGHTS_DATA) == 1968);
    static_assert(offsetof(LIGHTS_DATA, count) == 1952 && sizeof(SHADOW_DATA) == 400);
    static_assert(offsetof(MATERIAL_DATA, roughness) == 32 && offsetof(MATERIAL_DATA, specular) == 72);
    static_assert(sizeof(MATERIAL_DATA) == 176);
    constexpr size_t camera_offset = 0, light_offset = 256, world_offset = 2304, shadow_offset = 2560, material_offset = 3072;
    constexpr UINT descriptors_per_draw = 10;

    MATRIX4X4 transpose(const MATRIX4X4& matrix)
    {
        MATRIX4X4 result{};
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column) result.values[row][column] = matrix.values[column][row];
        return result;
    }

    MATERIAL_DATA material_data(const GLTF_MATERIAL& material, const MATERIAL_SETTINGS& settings)
    {
        MATERIAL_DATA data{};
        data.base_color = { material.base_color_factor[0] * settings.base_color.x,
            material.base_color_factor[1] * settings.base_color.y,
            material.base_color_factor[2] * settings.base_color.z,
            material.base_color_factor[3] * settings.base_color.w };
        data.emissive_metallic = { material.emissive_factor[0] * settings.emissive_multiplier,
            material.emissive_factor[1] * settings.emissive_multiplier,
            material.emissive_factor[2] * settings.emissive_multiplier,
            material.metallic_factor * settings.metallic_multiplier };
        data.roughness = material.roughness_factor * settings.roughness_multiplier;
        data.normal_scale = material.normal_scale;
        data.padding = material.occlusion_strength;
        data.has_base = !material.texture_paths[0].empty();
        data.has_metal = !material.texture_paths[1].empty();
        data.has_normal = !material.texture_paths[2].empty();
        data.has_ao = !material.texture_paths[3].empty();
        data.has_emissive = !material.texture_paths[4].empty();
        return data;
    }
}

bool PIP_FORWARD_PASS::initialize(ID3D12Device* device, bool isbackface_culling, bool ismatched)
{
    ComPtr<ID3DBlob> vertex, pixel;
    if (!SHADER::get_instance().compile_shader(ismatched ? L"Pip/MatchedForward.hlsl" : L"Pip/Gltf_Shader.hlsl",
        ismatched ? "VS_Matched" : "VS_GLTF", "vs_5_1", vertex) ||
        !SHADER::get_instance().compile_shader(ismatched ? L"Pip/MatchedForward.hlsl" : L"Pip/Gltf_Shader.hlsl",
            ismatched ? "PS_Matched" : "PS_GLTF", "ps_5_1", pixel)) return false;

    // Keep PIP's register assignments; append b6 and s2 for matched comparisons.
    D3D12_DESCRIPTOR_RANGE ranges[8]{};
    const UINT registers[] = {0,1,2,3,8,4,11,16};
    for (UINT index = 0; index < 8; ++index)
    {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].NumDescriptors = index == 4 ? 3 : 1;
        ranges[index].BaseShaderRegister = registers[index];
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    D3D12_ROOT_PARAMETER parameters[14]{};
    for (UINT index = 0; index < 4; ++index)
    {
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[index].Descriptor.ShaderRegister = index;
    }
    for (UINT index = 0; index < 6; ++index)
    {
        parameters[index + 4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index + 4].DescriptorTable = {1, &ranges[index]};
        parameters[index + 4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    parameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[10].Descriptor.ShaderRegister = 5;
    parameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[11].DescriptorTable = {2, &ranges[6]};
    parameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[12].Descriptor.ShaderRegister = 12;
    parameters[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[13].Descriptor.ShaderRegister = 6;
    parameters[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC samplers[3]{};
    samplers[0].Filter = ismatched ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_ANISOTROPIC;
    samplers[0].AddressU = samplers[0].AddressV = samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MaxAnisotropy = ismatched ? 1 : 16;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[1] = samplers[0];
    samplers[1].ShaderRegister = 1;
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[2] = samplers[0];
    samplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[2].ShaderRegister = 2;
    samplers[2].AddressU = samplers[2].AddressV = samplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    const D3D12_ROOT_SIGNATURE_DESC root{14, parameters, 3, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ComPtr<ID3DBlob> serialized, error;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error)))
    {
        if (error) OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        if (error) std::fputs(static_cast<const char*>(error->GetBufferPointer()), stderr);
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&_root)))) return false;
    // The shared importer packs TANGENT before UV; semantics are identical to PIP.
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,40,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = _root.Get();
    description.InputLayout = {layout, _countof(layout)};
    description.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    description.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    description.RasterizerState = CD3DX12_RASTERIZER_DESC();
    description.RasterizerState.CullMode = isbackface_culling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC();
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_pipeline)))) return false;
    description.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    _descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return SUCCEEDED(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&_wireframe)));
}

bool PIP_FORWARD_PASS::update(ID3D12Device* device, UINT frame_index, const SCENE& scene,
    const VIEWER_SETTINGS& settings, const MATRIX4X4& view, const MATRIX4X4& projection,
    const VECTOR3& camera, ID3D12Resource* environment, ID3D12Resource* specular, ID3D12Resource* brdf,
    bool isinstancing, bool iscached)
{
    if (frame_index >= frame_count || !environment || !specular || !brdf) return false;
    auto& frame = _frames[frame_index];
    frame.srv_writes = 0;
    struct GROUP { DRAW draw; MATERIAL_DATA material; std::vector<MATRIX4X4> matrices; };
    std::vector<GROUP> groups;
    std::unordered_map<std::string, size_t> lookup;
    for (const auto& object : scene.get_objects())
    {
        if (!object.isvisible || !object.model) continue;
        const auto& primitives = object.model->get_mesh().get_primitives();
        for (size_t index = 0; index < primitives.size(); ++index)
        {
            const auto& primitive = primitives[index];
            const auto world = multiply(primitive.node_transform, object.get_transform());
            const auto data = material_data(object.model->get_materials().at(primitive.material_index)->get_parameters(),
                object.materials.at(primitive.material_index));
            size_t group_index = groups.size();
            if (isinstancing)
            {
                const auto pointer = reinterpret_cast<uintptr_t>(object.model.get());
                std::string key(reinterpret_cast<const char*>(&pointer), sizeof(pointer));
                key.append(reinterpret_cast<const char*>(&index), sizeof(index));
                key.append(reinterpret_cast<const char*>(&data), sizeof(data));
                group_index = lookup.emplace(std::move(key), groups.size()).first->second;
            }
            if (group_index == groups.size()) groups.push_back({{object.model, static_cast<UINT>(index)}, data, {}});
            groups[group_index].matrices.push_back(transpose(world));
        }
    }
    if (groups.size() > 65536) return false;
    frame.draws.clear();
    size_t matrix_count = 0;
    for (const auto& group : groups) matrix_count += group.matrices.size();
    frame.instance_offset = material_offset + groups.size() * 256;
    const size_t bytes = frame.instance_offset + matrix_count * sizeof(MATRIX4X4);
    if (frame.capacity < bytes)
    {
        if (frame.buffer && frame.mapped) frame.buffer->Unmap(0, nullptr);
        frame.mapped = nullptr;
        frame.buffer.Reset();
        frame.capacity = 0;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        const auto description = CD3DX12_RESOURCE_DESC::buffer(bytes);
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frame.buffer)))) return false;
        const D3D12_RANGE read{0,0};
        if (FAILED(frame.buffer->Map(0, &read, reinterpret_cast<void**>(&frame.mapped)))) return false;
        frame.capacity = bytes;
    }
    const size_t descriptors = (std::max)(size_t{1}, groups.size() * descriptors_per_draw);
    if (frame.descriptor_capacity < descriptors)
    {
        frame.heap.Reset();
        frame.descriptor_capacity = 0;
        CD3DX12_DESCRIPTOR_HEAP_DESC description(static_cast<UINT>(descriptors), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&frame.heap)))) return false;
        frame.descriptor_capacity = descriptors;
        frame.descriptor_resources.clear();
    }
    const CAMERA_DATA camera_data{transpose(view), transpose(projection), {camera.x,camera.y,camera.z,1}};
    const WORLD_DATA world_data{transpose(identity_matrix()), transpose(identity_matrix()), 0, -1, {}};
    const SHADOW_DATA shadow_data{};
    LIGHTS_DATA lights{};
    lights.ambient = {0.2f,0.2f,0.2f,1};
    lights.lights[0].isenabled = 1;
    lights.lights[0].type = 3;
    lights.lights[0].direction = settings.light_direction;
    lights.lights[0].diffuse = {settings.light_color.x * settings.light_intensity,
        settings.light_color.y * settings.light_intensity, settings.light_color.z * settings.light_intensity, 1};
    const VECTOR4 sh[] = {
        {0.384992f,0.443180f,0.523824f,0}, {0.048705f,0.059345f,0.076326f,0},
        {-0.089855f,-0.106461f,-0.133221f,0}, {0.012586f,0.014529f,0.017559f,0},
        {-0.017367f,-0.021074f,-0.027003f,0}, {-0.006745f,-0.007963f,-0.010173f,0},
        {0.038166f,0.044129f,0.052675f,0}, {0.002820f,0.003429f,0.004455f,0},
        {-0.040182f,-0.049419f,-0.064508f,0}
    };
    std::memcpy(lights.sh, sh, sizeof(sh));
    std::memcpy(frame.mapped + camera_offset, &camera_data, sizeof(camera_data));
    std::memcpy(frame.mapped + light_offset, &lights, sizeof(lights));
    std::memcpy(frame.mapped + world_offset, &world_data, sizeof(world_data));
    std::memcpy(frame.mapped + shadow_offset, &shadow_data, sizeof(shadow_data));
    auto handle = frame.heap->GetCPUDescriptorHandleForHeapStart();
    frame.descriptor_resources.resize(groups.size());
    UINT first_instance = 0;
    for (size_t index = 0; index < groups.size(); ++index)
    {
        auto& group = groups[index];
        group.draw.first_instance = first_instance;
        group.draw.instance_count = static_cast<UINT>(group.matrices.size());
        std::memcpy(frame.mapped + material_offset + index * 256, &group.material, sizeof(group.material));
        std::memcpy(frame.mapped + frame.instance_offset + first_instance * sizeof(MATRIX4X4),
            group.matrices.data(), group.matrices.size() * sizeof(MATRIX4X4));
        first_instance += group.draw.instance_count;
        const auto material_index = group.draw.model->get_mesh().get_primitives()[group.draw.primitive].material_index;
        const auto& textures = group.draw.model->get_materials()[material_index]->get_textures();
        std::array<ID3D12Resource*, 10> resources = {textures.get_texture(0), textures.get_texture(2), textures.get_texture(1),
            textures.get_texture(4), environment, specular, brdf, textures.get_texture(3), nullptr, nullptr};
        if (iscached && frame.descriptor_resources[index] == resources)
        {
            handle.ptr += descriptors_per_draw * _descriptor_size;
            frame.draws.push_back(std::move(group.draw));
            continue;
        }
        frame.descriptor_resources[index] = resources;
        for (UINT slot = 0; slot < descriptors_per_draw; ++slot)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC description{};
            description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (slot >= 8)
            {
                description.Format = DXGI_FORMAT_R32_FLOAT;
                description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                description.Texture2DArray.ArraySize = 3;
                description.Texture2DArray.MipLevels = 1;
            }
            else if (slot == 4 || slot == 5)
            {
                description.Format = resources[slot]->GetDesc().Format;
                description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                description.TextureCube.MipLevels = resources[slot]->GetDesc().MipLevels;
            }
            else
            {
                description.Format = slot == 6 ? resources[slot]->GetDesc().Format :
                    (slot == 0 || slot == 3 ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
                description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                description.Texture2D.MipLevels = 1;
            }
            device->CreateShaderResourceView(resources[slot], &description, handle);
            ++frame.srv_writes;
            handle.ptr += _descriptor_size;
        }
        frame.draws.push_back(std::move(group.draw));
    }
    return true;
}

void PIP_FORWARD_PASS::render(ID3D12GraphicsCommandList* list, UINT frame_index, bool iswireframe,
    D3D12_GPU_VIRTUAL_ADDRESS lights) const
{
    const auto& frame = _frames[frame_index];
    if (frame.draws.empty()) return;
    list->SetPipelineState(iswireframe ? _wireframe.Get() : _pipeline.Get());
    list->SetGraphicsRootSignature(_root.Get());
    ID3D12DescriptorHeap* heaps[] = {frame.heap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto base = frame.buffer->GetGPUVirtualAddress();
    list->SetGraphicsRootConstantBufferView(0, base + world_offset);
    list->SetGraphicsRootConstantBufferView(1, base + camera_offset);
    list->SetGraphicsRootConstantBufferView(3, base + light_offset);
    list->SetGraphicsRootConstantBufferView(10, base + shadow_offset);
    if (lights) list->SetGraphicsRootConstantBufferView(13, lights);
    for (size_t index = 0; index < frame.draws.size(); ++index)
    {
        const auto& draw = frame.draws[index];
        const auto handle = [&](UINT slot)
        {
            auto result = frame.heap->GetGPUDescriptorHandleForHeapStart();
            result.ptr += (index * descriptors_per_draw + slot) * _descriptor_size;
            return result;
        };
        list->SetGraphicsRootConstantBufferView(2, base + material_offset + index * 256);
        for (UINT slot = 0; slot < 4; ++slot) list->SetGraphicsRootDescriptorTable(4 + slot, handle(slot));
        list->SetGraphicsRootDescriptorTable(8, handle(4));
        list->SetGraphicsRootDescriptorTable(9, handle(7));
        list->SetGraphicsRootDescriptorTable(11, handle(8));
        list->SetGraphicsRootShaderResourceView(12, base + frame.instance_offset + draw.first_instance * sizeof(MATRIX4X4));
        const auto& geometry = draw.model->get_geometry();
        list->IASetVertexBuffers(0, 1, &geometry.get_vertex_view());
        list->IASetIndexBuffer(&geometry.get_index_view());
        const auto& primitive = draw.model->get_mesh().get_primitives()[draw.primitive];
        list->DrawIndexedInstanced(primitive.index_count, draw.instance_count, primitive.first_index, 0, 0);
    }
}

void PIP_FORWARD_PASS::reset()
{
    for (auto& frame : _frames)
    {
        if (frame.buffer && frame.mapped) frame.buffer->Unmap(0, nullptr);
        frame = {};
    }
    _root.Reset();
    _pipeline.Reset();
    _wireframe.Reset();
}
