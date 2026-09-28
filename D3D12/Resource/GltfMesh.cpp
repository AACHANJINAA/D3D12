#include "../Common/stdafx.h"
#include "GltfMesh.h"

namespace
{
    struct ACCESSOR_INFO
    {
        size_t buffer_view = 0;
        size_t count = 0;
    };

    struct BUFFER_VIEW_INFO
    {
        size_t byte_offset = 0;
        size_t byte_length = 0;
    };

    std::vector<std::string> get_objects(const std::string& json, const char* key)
    {
        const size_t key_position = json.find(key);
        if (key_position == std::string::npos)
        {
            return {};
        }

        const size_t array_start = json.find('[', key_position);
        if (array_start == std::string::npos)
        {
            return {};
        }

        std::vector<std::string> objects;
        size_t object_start = std::string::npos;
        int brace_depth = 0;
        for (size_t index = array_start + 1; index < json.size(); ++index)
        {
            if (json[index] == '{')
            {
                if (brace_depth == 0)
                {
                    object_start = index;
                }
                ++brace_depth;
            }
            else if (json[index] == '}')
            {
                --brace_depth;
                if (brace_depth == 0 && object_start != std::string::npos)
                {
                    objects.push_back(json.substr(object_start, index - object_start + 1));
                    object_start = std::string::npos;
                }
            }
            else if (json[index] == ']' && brace_depth == 0)
            {
                break;
            }
        }
        return objects;
    }

    bool read_number(const std::string& object, const char* key, size_t& value)
    {
        const size_t key_position = object.find(key);
        if (key_position == std::string::npos)
        {
            return false;
        }

        const size_t value_start = object.find(':', key_position);
        if (value_start == std::string::npos)
        {
            return false;
        }

        try
        {
            value = std::stoull(object.substr(value_start + 1));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    bool read_string(const std::string& object, const char* key, std::string& value)
    {
        const size_t key_position = object.find(key);
        if (key_position == std::string::npos)
        {
            return false;
        }

        const size_t first_quote = object.find('"', object.find(':', key_position) + 1);
        if (first_quote == std::string::npos)
        {
            return false;
        }
        const size_t second_quote = object.find('"', first_quote + 1);
        if (second_quote == std::string::npos)
        {
            return false;
        }
        value = object.substr(first_quote + 1, second_quote - first_quote - 1);
        return true;
    }

    bool read_float(const std::string& object, const char* key, float& value)
    {
        const size_t key_position = object.find(key);
        if (key_position == std::string::npos)
        {
            return false;
        }
        const size_t value_start = object.find(':', key_position);
        if (value_start == std::string::npos)
        {
            return false;
        }
        try
        {
            value = std::stof(object.substr(value_start + 1));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    bool read_float_array(const std::string& object, const char* key, float* values, size_t count)
    {
        const size_t key_position = object.find(key);
        if (key_position == std::string::npos)
        {
            return false;
        }
        const size_t array_start = object.find('[', key_position);
        const size_t array_end = object.find(']', array_start);
        if (array_start == std::string::npos || array_end == std::string::npos)
        {
            return false;
        }
        std::string array_text = object.substr(array_start + 1, array_end - array_start - 1);
        size_t offset = 0;
        try
        {
            for (size_t index = 0; index < count; ++index)
            {
                const size_t separator = array_text.find(',', offset);
                const size_t length = separator == std::string::npos
                    ? array_text.size() - offset
                    : separator - offset;
                values[index] = std::stof(array_text.substr(offset, length));
                if (separator == std::string::npos)
                {
                    break;
                }
                offset = separator + 1;
            }
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    template <typename T>
    bool read_binary(const std::vector<uint8_t>& binary,
        size_t offset, size_t count, std::vector<T>& output)
    {
        const size_t byte_size = sizeof(T) * count;
        if (offset > binary.size() || byte_size > binary.size() - offset)
        {
            return false;
        }
        output.resize(count);
        std::memcpy(output.data(), binary.data() + offset, byte_size);
        return true;
    }
}

bool GLTF_MESH::load(const std::filesystem::path& file_path)
{
    _vertices.clear();
    _indices.clear();

    std::ifstream gltf_file(file_path);
    if (!gltf_file)
    {
        return false;
    }
    const std::string json((std::istreambuf_iterator<char>(gltf_file)), {});
    // glTF defaults: white base color, metallic/roughness factors of one.
    _material = {};
    _material.base_color_factor[0] = 1.0f;
    _material.base_color_factor[1] = 1.0f;
    _material.base_color_factor[2] = 1.0f;
    _material.base_color_factor[3] = 1.0f;
    _material.metallic_factor = 1.0f;
    _material.roughness_factor = 1.0f;

    const auto material_objects = get_objects(json, "\"materials\"");
    if (!material_objects.empty())
    {
        const std::string& material = material_objects[0];
        read_float_array(material, "\"baseColorFactor\"", _material.base_color_factor, 4);
        read_float_array(material, "\"emissiveFactor\"", _material.emissive_factor, 3);
        read_float(material, "\"metallicFactor\"", _material.metallic_factor);
        read_float(material, "\"roughnessFactor\"", _material.roughness_factor);
    }

    const auto buffer_objects = get_objects(json, "\"buffers\"");
    const auto buffer_view_objects = get_objects(json, "\"bufferViews\"");
    const auto accessor_objects = get_objects(json, "\"accessors\"");
    if (buffer_objects.empty() || buffer_view_objects.size() < 4 || accessor_objects.size() < 4)
    {
        return false;
    }

    std::string buffer_uri;
    if (!read_string(buffer_objects[0], "\"uri\"", buffer_uri))
    {
        return false;
    }

    std::ifstream binary_file(file_path.parent_path() / buffer_uri, std::ios::binary);
    if (!binary_file)
    {
        return false;
    }
    const std::vector<uint8_t> binary(
        (std::istreambuf_iterator<char>(binary_file)), {});

    std::vector<BUFFER_VIEW_INFO> buffer_views(buffer_view_objects.size());
    for (size_t index = 0; index < buffer_view_objects.size(); ++index)
    {
        read_number(buffer_view_objects[index], "\"byteOffset\"", buffer_views[index].byte_offset);
        if (!read_number(buffer_view_objects[index], "\"byteLength\"", buffer_views[index].byte_length))
        {
            return false;
        }
    }

    std::vector<ACCESSOR_INFO> accessors(accessor_objects.size());
    for (size_t index = 0; index < accessor_objects.size(); ++index)
    {
        if (!read_number(accessor_objects[index], "\"bufferView\"", accessors[index].buffer_view) ||
            !read_number(accessor_objects[index], "\"count\"", accessors[index].count))
        {
            return false;
        }
    }

    std::vector<uint16_t> source_indices;
    std::vector<float> source_positions;
    std::vector<float> source_normals;
    std::vector<float> source_uvs;
    if (!read_binary(binary,
        buffer_views[accessors[0].buffer_view].byte_offset,
        accessors[0].count, source_indices) ||
        !read_binary(binary,
        buffer_views[accessors[1].buffer_view].byte_offset,
        accessors[1].count * 3, source_positions) ||
        !read_binary(binary,
        buffer_views[accessors[2].buffer_view].byte_offset,
        accessors[2].count * 3, source_normals) ||
        !read_binary(binary,
        buffer_views[accessors[3].buffer_view].byte_offset,
        accessors[3].count * 2, source_uvs))
    {
        return false;
    }

    const size_t vertex_count = accessors[1].count;
    if (accessors[2].count != vertex_count || accessors[3].count != vertex_count)
    {
        return false;
    }

    _vertices.resize(vertex_count);
    const float node_rotation_x = MATH::pi * 0.5f;
    const float rotation_cosine = std::cos(node_rotation_x);
    const float rotation_sine = std::sin(node_rotation_x);
    const float model_rotation_y = MATH::pi;
    const float model_rotation_cosine = std::cos(model_rotation_y);
    const float model_rotation_sine = std::sin(model_rotation_y);
    for (size_t index = 0; index < vertex_count; ++index)
    {
        std::memcpy(_vertices[index].position, source_positions.data() + index * 3, sizeof(float) * 3);
        std::memcpy(_vertices[index].normal, source_normals.data() + index * 3, sizeof(float) * 3);
        std::memcpy(_vertices[index].uv, source_uvs.data() + index * 2, sizeof(float) * 2);

        const float position_y = _vertices[index].position[1];
        const float position_z = _vertices[index].position[2];
        _vertices[index].position[1] = position_y * rotation_cosine - position_z * rotation_sine;
        _vertices[index].position[2] = position_y * rotation_sine + position_z * rotation_cosine;

        const float rotated_position_x = _vertices[index].position[0];
        const float rotated_position_z = _vertices[index].position[2];
        _vertices[index].position[0] = rotated_position_x * model_rotation_cosine +
            rotated_position_z * model_rotation_sine;
        _vertices[index].position[2] = -rotated_position_x * model_rotation_sine +
            rotated_position_z * model_rotation_cosine;

        const float normal_y = _vertices[index].normal[1];
        const float normal_z = _vertices[index].normal[2];
        _vertices[index].normal[1] = normal_y * rotation_cosine - normal_z * rotation_sine;
        _vertices[index].normal[2] = normal_y * rotation_sine + normal_z * rotation_cosine;

        const float rotated_normal_x = _vertices[index].normal[0];
        const float rotated_normal_z = _vertices[index].normal[2];
        _vertices[index].normal[0] = rotated_normal_x * model_rotation_cosine +
            rotated_normal_z * model_rotation_sine;
        _vertices[index].normal[2] = -rotated_normal_x * model_rotation_sine +
            rotated_normal_z * model_rotation_cosine;
    }

    _indices.assign(source_indices.begin(), source_indices.end());
    std::vector<float> bitangent_accumulation(_vertices.size() * 3, 0.0f);

    for (size_t index = 0; index + 2 < _indices.size(); index += 3)
    {
        GLTF_VERTEX& vertex0 = _vertices[_indices[index]];
        GLTF_VERTEX& vertex1 = _vertices[_indices[index + 1]];
        GLTF_VERTEX& vertex2 = _vertices[_indices[index + 2]];
        const float edge1[3]
        {
            vertex1.position[0] - vertex0.position[0],
            vertex1.position[1] - vertex0.position[1],
            vertex1.position[2] - vertex0.position[2]
        };
        const float edge2[3]
        {
            vertex2.position[0] - vertex0.position[0],
            vertex2.position[1] - vertex0.position[1],
            vertex2.position[2] - vertex0.position[2]
        };
        const float delta_uv1[2]
        {
            vertex1.uv[0] - vertex0.uv[0], vertex1.uv[1] - vertex0.uv[1]
        };
        const float delta_uv2[2]
        {
            vertex2.uv[0] - vertex0.uv[0], vertex2.uv[1] - vertex0.uv[1]
        };
        const float determinant = delta_uv1[0] * delta_uv2[1] -
            delta_uv2[0] * delta_uv1[1];
        if (std::abs(determinant) <= 0.000001f)
        {
            continue;
        }

        const float inverse_determinant = 1.0f / determinant;
        const float tangent[3]
        {
            inverse_determinant * (delta_uv2[1] * edge1[0] - delta_uv1[1] * edge2[0]),
            inverse_determinant * (delta_uv2[1] * edge1[1] - delta_uv1[1] * edge2[1]),
            inverse_determinant * (delta_uv2[1] * edge1[2] - delta_uv1[1] * edge2[2])
        };
        const float bitangent[3]
        {
            inverse_determinant * (delta_uv1[0] * edge2[0] - delta_uv2[0] * edge1[0]),
            inverse_determinant * (delta_uv1[0] * edge2[1] - delta_uv2[0] * edge1[1]),
            inverse_determinant * (delta_uv1[0] * edge2[2] - delta_uv2[0] * edge1[2])
        };
        for (uint32_t vertex_index : { _indices[index], _indices[index + 1], _indices[index + 2] })
        {
            _vertices[vertex_index].tangent[0] += tangent[0];
            _vertices[vertex_index].tangent[1] += tangent[1];
            _vertices[vertex_index].tangent[2] += tangent[2];
            bitangent_accumulation[vertex_index * 3 + 0] += bitangent[0];
            bitangent_accumulation[vertex_index * 3 + 1] += bitangent[1];
            bitangent_accumulation[vertex_index * 3 + 2] += bitangent[2];
        }
    }

    for (size_t vertex_index = 0; vertex_index < _vertices.size(); ++vertex_index)
    {
        GLTF_VERTEX& vertex = _vertices[vertex_index];
        const float normal_dot_tangent =
            vertex.normal[0] * vertex.tangent[0] +
            vertex.normal[1] * vertex.tangent[1] +
            vertex.normal[2] * vertex.tangent[2];
        vertex.tangent[0] -= vertex.normal[0] * normal_dot_tangent;
        vertex.tangent[1] -= vertex.normal[1] * normal_dot_tangent;
        vertex.tangent[2] -= vertex.normal[2] * normal_dot_tangent;
        const float tangent_length = std::sqrt(
            vertex.tangent[0] * vertex.tangent[0] +
            vertex.tangent[1] * vertex.tangent[1] +
            vertex.tangent[2] * vertex.tangent[2]);
        if (tangent_length > 0.000001f)
        {
            vertex.tangent[0] /= tangent_length;
            vertex.tangent[1] /= tangent_length;
            vertex.tangent[2] /= tangent_length;
        }
        else
        {
            vertex.tangent[0] = 1.0f;
            vertex.tangent[1] = 0.0f;
            vertex.tangent[2] = 0.0f;
        }
        const float bitangent_x =
            vertex.normal[1] * vertex.tangent[2] - vertex.normal[2] * vertex.tangent[1];
        const float bitangent_y =
            vertex.normal[2] * vertex.tangent[0] - vertex.normal[0] * vertex.tangent[2];
        const float bitangent_z =
            vertex.normal[0] * vertex.tangent[1] - vertex.normal[1] * vertex.tangent[0];
        const float handedness =
            bitangent_x * bitangent_accumulation[vertex_index * 3 + 0] +
            bitangent_y * bitangent_accumulation[vertex_index * 3 + 1] +
            bitangent_z * bitangent_accumulation[vertex_index * 3 + 2];
        vertex.tangent[3] = handedness < 0.0f ? -1.0f : 1.0f;
    }
    return true;
}

const std::vector<GLTF_VERTEX>& GLTF_MESH::get_vertices() const
{
    return _vertices;
}

const std::vector<uint32_t>& GLTF_MESH::get_indices() const
{
    return _indices;
}

const GLTF_MATERIAL& GLTF_MESH::get_material() const
{
    return _material;
}
