#include "../Common/stdafx.h"
#include "GltfMesh.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cfloat>
#include <cwchar>
#include <cwctype>
#include <stdexcept>
#include <map>

namespace
{
    using winrt::Windows::Data::Json::JsonObject;
    using winrt::Windows::Data::Json::JsonArray;

    void require(bool isvalid, const char* message)
    {
        if (!isvalid) throw std::runtime_error(message);
    }

    size_t integer(double value)
    {
        require(std::isfinite(value) && value >= 0 && value <= UINT_MAX &&
            value == std::floor(value), "Invalid integer in glTF.");
        return static_cast<size_t>(value);
    }

    size_t number(const JsonObject& object, const wchar_t* key, size_t fallback = 0)
    {
        return object.HasKey(key) ? integer(object.GetNamedNumber(key)) : fallback;
    }

    JsonObject object_at(const JsonArray& array, size_t index)
    {
        require(index < array.Size(), "glTF index is out of bounds.");
        return array.GetObjectAt(static_cast<uint32_t>(index));
    }

    float finite_float(double value)
    {
        require(std::isfinite(value) && std::abs(value) <= FLT_MAX, "Non-finite glTF value.");
        return static_cast<float>(value);
    }

    void floats(const JsonObject& object, const wchar_t* key, float* values, size_t count)
    {
        if (!object.HasKey(key)) return;
        const auto array = object.GetNamedArray(key);
        require(array.Size() == count, "Incorrect vector/matrix length.");
        for (size_t index = 0; index < count; ++index)
            values[index] = finite_float(array.GetNumberAt(static_cast<uint32_t>(index)));
    }

    std::vector<uint8_t> read_file(const std::filesystem::path& path, size_t limit)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        require(static_cast<bool>(file), "A referenced glTF or buffer file could not be opened.");
        const auto length = file.tellg();
        require(length > 0 && static_cast<uint64_t>(length) <= limit, "Empty or oversized input file.");
        std::vector<uint8_t> bytes(static_cast<size_t>(length));
        file.seekg(0);
        require(static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))), "Could not read the complete input file.");
        return bytes;
    }

    std::filesystem::path uri_path(const std::filesystem::path& directory, const winrt::hstring& uri)
    {
        const std::string encoded = winrt::to_string(uri);
        std::string decoded;
        auto hex = [](char value) -> int
        {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        for (size_t index = 0; index < encoded.size(); ++index)
        {
            char value = encoded[index];
            if (value == '%')
            {
                require(index + 2 < encoded.size(), "Invalid URI escape.");
                const int high = hex(encoded[index + 1]), low = hex(encoded[index + 2]);
                require(high >= 0 && low >= 0, "Invalid URI escape.");
                value = static_cast<char>(high * 16 + low);
                index += 2;
            }
            require(value != '\0', "Invalid URI.");
            decoded += value;
        }
        require(!decoded.empty() && decoded.find(':') == std::string::npos &&
            decoded.find('?') == std::string::npos && decoded.find('#') == std::string::npos,
            "Only local external buffer/image URIs are supported (no data URI or URL).");
        const auto relative = std::filesystem::path(winrt::to_hstring(decoded).c_str());
        require(!relative.is_absolute() && !relative.has_root_path(), "Expected a relative asset URI.");
        return directory / relative;
    }

    struct ACCESSOR
    {
        const uint8_t* data = nullptr;
        size_t count = 0;
        size_t stride = 0;
        size_t component_size = 0;
        size_t components = 0;
        size_t component_type = 0;
        bool isnormalized = false;

        double read(size_t element, size_t component) const
        {
            const uint8_t* source = data + element * stride + component * component_size;
            if (component_type == 5126)
            {
                float value;
                std::memcpy(&value, source, sizeof(value));
                require(std::isfinite(value), "Non-finite vertex data.");
                return value;
            }
            uint32_t value = 0;
            std::memcpy(&value, source, component_size);
            if (isnormalized) return value / (component_type == 5121 ? 255.0 : 65535.0);
            return value;
        }
    };

    ACCESSOR accessor(const JsonObject& json, size_t index,
        const std::vector<std::vector<uint8_t>>& buffers, const wchar_t* type)
    {
        const auto item = object_at(json.GetNamedArray(L"accessors"), index);
        require(!item.HasKey(L"sparse"), "Sparse accessors are not supported yet.");
        require(item.GetNamedString(L"type") == type, "Unexpected accessor shape.");
        const auto view = object_at(json.GetNamedArray(L"bufferViews"), number(item, L"bufferView", UINT_MAX));
        require(!view.HasKey(L"extensions"), "Compressed/extension buffer views are not supported.");
        const size_t buffer_index = number(view, L"buffer", UINT_MAX);
        require(buffer_index < buffers.size(), "Invalid buffer index.");
        const auto& bytes = buffers[buffer_index];
        ACCESSOR result;
        result.count = number(item, L"count");
        result.components = std::wcscmp(type, L"VEC3") == 0 ? 3
            : std::wcscmp(type, L"VEC2") == 0 ? 2 : std::wcscmp(type, L"VEC4") == 0 ? 4 : 1;
        result.component_type = number(item, L"componentType");
        require(result.component_type == 5121 || result.component_type == 5123 ||
            result.component_type == 5125 || result.component_type == 5126,
            "Unsupported accessor component type.");
        result.component_size = result.component_type == 5121 ? 1 : result.component_type == 5123 ? 2 : 4;
        result.isnormalized = item.GetNamedBoolean(L"normalized", false);
        require(!result.isnormalized || result.component_type == 5121 || result.component_type == 5123,
            "Invalid normalized accessor.");
        const size_t packed = result.component_size * result.components;
        result.stride = number(view, L"byteStride", packed);
        require(result.stride >= packed && result.stride % result.component_size == 0,
            "Invalid accessor stride.");
        require(!view.HasKey(L"byteStride") || (result.stride >= 4 && result.stride <= 252 && result.stride % 4 == 0),
            "Invalid glTF byteStride.");
        const size_t start = number(view, L"byteOffset");
        const size_t length = number(view, L"byteLength");
        const size_t offset = number(item, L"byteOffset");
        require(start <= bytes.size() && length <= bytes.size() - start &&
            offset <= length && result.count > 0 && packed <= length - offset,
            "Accessor exceeds buffer bounds.");
        require(result.count - 1 <= (length - offset - packed) / result.stride,
            "Accessor count exceeds buffer view bounds.");
        result.data = bytes.data() + start + offset;
        return result;
    }

    MATH::MATRIX4X4 node_matrix(const JsonObject& node)
    {
        auto matrix = MATH::identity_matrix();
        if (node.HasKey(L"matrix"))
        {
            require(!node.HasKey(L"rotation") && !node.HasKey(L"translation") && !node.HasKey(L"scale"),
                "A node cannot contain both matrix and TRS.");
            floats(node, L"matrix", &matrix.values[0][0], 16);
            require(matrix.values[0][3] == 0 && matrix.values[1][3] == 0 &&
                matrix.values[2][3] == 0 && matrix.values[3][3] == 1, "Non-affine node matrix.");
            return matrix;
        }
        float rotation[4]{ 0, 0, 0, 1 }, scale[3]{ 1, 1, 1 }, translation[3]{};
        floats(node, L"rotation", rotation, 4);
        floats(node, L"scale", scale, 3);
        floats(node, L"translation", translation, 3);
        const float length = std::sqrt(rotation[0]*rotation[0] + rotation[1]*rotation[1] +
            rotation[2]*rotation[2] + rotation[3]*rotation[3]);
        require(length > 0.000001f, "Invalid node quaternion.");
        for (float& value : rotation) value /= length;
        const float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
        matrix.values[0][0] = (1 - 2*y*y - 2*z*z) * scale[0];
        matrix.values[0][1] = (2*x*y + 2*z*w) * scale[0];
        matrix.values[0][2] = (2*x*z - 2*y*w) * scale[0];
        matrix.values[1][0] = (2*x*y - 2*z*w) * scale[1];
        matrix.values[1][1] = (1 - 2*x*x - 2*z*z) * scale[1];
        matrix.values[1][2] = (2*y*z + 2*x*w) * scale[1];
        matrix.values[2][0] = (2*x*z + 2*y*w) * scale[2];
        matrix.values[2][1] = (2*y*z - 2*x*w) * scale[2];
        matrix.values[2][2] = (1 - 2*x*x - 2*y*y) * scale[2];
        for (size_t axis = 0; axis < 3; ++axis) matrix.values[3][axis] = translation[axis];
        return matrix;
    }

    MATH::VECTOR3 vector(const float* value) { return { value[0], value[1], value[2] }; }
    void write(float* output, const MATH::VECTOR3& value)
    {
        output[0] = value.x; output[1] = value.y; output[2] = value.z;
    }
    MATH::VECTOR3 direction(const MATH::MATRIX4X4& matrix, const MATH::VECTOR3& value)
    {
        return {
            value.x * matrix.values[0][0] + value.y * matrix.values[1][0] + value.z * matrix.values[2][0],
            value.x * matrix.values[0][1] + value.y * matrix.values[1][1] + value.z * matrix.values[2][1],
            value.x * matrix.values[0][2] + value.y * matrix.values[1][2] + value.z * matrix.values[2][2] };
    }

    void read_material(const JsonObject& json, const JsonObject& primitive,
        const std::filesystem::path& directory, GLTF_MATERIAL& output)
    {
        output.name = "Default";
        if (!primitive.HasKey(L"material")) return;
        const auto material = object_at(json.GetNamedArray(L"materials"), number(primitive, L"material"));
        output.name = winrt::to_string(material.GetNamedString(L"name", L"Material"));
        require(material.GetNamedString(L"alphaMode", L"OPAQUE") == L"OPAQUE",
            "Alpha MASK/BLEND materials are not supported yet.");
        require(!material.HasKey(L"extensions"), "Material extensions are not supported yet.");
        floats(material, L"emissiveFactor", output.emissive_factor, 3);
        auto texture = [&](const JsonObject& owner, const wchar_t* key, size_t slot)
        {
            if (!owner.HasKey(key)) return;
            const auto info = owner.GetNamedObject(key);
            require(number(info, L"texCoord") == 0 && !info.HasKey(L"extensions"),
                "Only TEXCOORD_0 without texture transforms is supported.");
            const auto item = object_at(json.GetNamedArray(L"textures"), number(info, L"index", UINT_MAX));
            require(!item.HasKey(L"extensions"), "Compressed/extension textures are not supported.");
            if (item.HasKey(L"sampler"))
            {
                const auto sampler = object_at(json.GetNamedArray(L"samplers"), number(item, L"sampler"));
                require(number(sampler, L"wrapS", 10497) == 10497 &&
                    number(sampler, L"wrapT", 10497) == 10497,
                    "This importer currently supports repeat texture wrapping only.");
            }
            const auto image = object_at(json.GetNamedArray(L"images"), number(item, L"source", UINT_MAX));
            require(image.HasKey(L"uri") && !image.HasKey(L"bufferView"),
                "Embedded images are not supported; use external PNG/JPEG files.");
            output.texture_paths[slot] = uri_path(directory, image.GetNamedString(L"uri"));
            if (slot == 1) output.normal_scale = finite_float(info.GetNamedNumber(L"scale", 1));
            if (slot == 3) output.occlusion_strength = finite_float(info.GetNamedNumber(L"strength", 1));
        };
        if (material.HasKey(L"pbrMetallicRoughness"))
        {
            const auto pbr = material.GetNamedObject(L"pbrMetallicRoughness");
            floats(pbr, L"baseColorFactor", output.base_color_factor, 4);
            output.metallic_factor = finite_float(pbr.GetNamedNumber(L"metallicFactor", 1));
            output.roughness_factor = finite_float(pbr.GetNamedNumber(L"roughnessFactor", 1));
            texture(pbr, L"baseColorTexture", 0);
            texture(pbr, L"metallicRoughnessTexture", 2);
        }
        texture(material, L"normalTexture", 1);
        texture(material, L"occlusionTexture", 3);
        texture(material, L"emissiveTexture", 4);
        require(output.metallic_factor >= 0 && output.metallic_factor <= 1 &&
            output.roughness_factor >= 0 && output.roughness_factor <= 1 &&
            output.occlusion_strength >= 0 && output.occlusion_strength <= 1,
            "Material factors must be in the range 0..1.");
    }
    void read_geometry(const JsonObject& json, const JsonObject& primitive,
        const std::vector<std::vector<uint8_t>>& buffers, const GLTF_MATERIAL& material,
        std::vector<GLTF_VERTEX>& vertices, std::vector<uint32_t>& indices)
    {
        const auto attributes = primitive.GetNamedObject(L"attributes");
        require(!attributes.HasKey(L"COLOR_0") && !attributes.HasKey(L"JOINTS_0"),
            "Vertex colors and skin attributes are not supported yet.");
        const auto positions = accessor(json, number(attributes, L"POSITION", UINT_MAX), buffers, L"VEC3");
        require(positions.component_type == 5126 && positions.count <= 4000000,
            "POSITION must be FLOAT VEC3, with at most 4 million vertices.");
        vertices.resize(positions.count);
        const bool isnormal = attributes.HasKey(L"NORMAL"), isuv = attributes.HasKey(L"TEXCOORD_0");
        const bool istangent = attributes.HasKey(L"TANGENT");
        ACCESSOR normals, uvs, tangents;
        if (isnormal)
        {
            normals = accessor(json, number(attributes, L"NORMAL"), buffers, L"VEC3");
            require(normals.count == positions.count && normals.component_type == 5126, "Invalid NORMAL accessor.");
        }
        if (isuv)
        {
            uvs = accessor(json, number(attributes, L"TEXCOORD_0"), buffers, L"VEC2");
            require(uvs.count == positions.count && (uvs.component_type == 5126 ||
                (uvs.isnormalized && (uvs.component_type == 5121 || uvs.component_type == 5123))),
                "Invalid TEXCOORD_0 accessor.");
        }
        else
            for (const auto& texture : material.texture_paths)
                require(texture.empty(), "Textured material requires TEXCOORD_0.");
        if (istangent)
        {
            tangents = accessor(json, number(attributes, L"TANGENT"), buffers, L"VEC4");
            require(isnormal && tangents.count == positions.count && tangents.component_type == 5126,
                "Invalid TANGENT accessor.");
        }

        auto world = MATH::identity_matrix();
        // Convert geometry once; each node keeps its own transform.
        for (auto& row : world.values) row[2] = -row[2];
        const auto a = vector(world.values[0]), b = vector(world.values[1]), c = vector(world.values[2]);
        const float determinant = MATH::dot(a, MATH::cross(b, c));
        require(std::isfinite(determinant) && std::abs(determinant) > 1e-12f, "Singular node transform.");
        auto normal_matrix = MATH::identity_matrix();
        write(normal_matrix.values[0], MATH::multiply(MATH::cross(b, c), 1 / determinant));
        write(normal_matrix.values[1], MATH::multiply(MATH::cross(c, a), 1 / determinant));
        write(normal_matrix.values[2], MATH::multiply(MATH::cross(a, b), 1 / determinant));
        for (size_t index = 0; index < positions.count; ++index)
        {
            auto& vertex = vertices[index];
            MATH::VECTOR3 position{ static_cast<float>(positions.read(index, 0)),
                static_cast<float>(positions.read(index, 1)), static_cast<float>(positions.read(index, 2)) };
            write(vertex.position, MATH::add(direction(world, position), vector(world.values[3])));
            for (float value : vertex.position) require(std::isfinite(value), "Invalid transformed position.");
            if (isnormal)
            {
                MATH::VECTOR3 normal{ static_cast<float>(normals.read(index, 0)),
                    static_cast<float>(normals.read(index, 1)), static_cast<float>(normals.read(index, 2)) };
                write(vertex.normal, MATH::normalize(direction(normal_matrix, normal)));
            }
            if (isuv) for (size_t axis = 0; axis < 2; ++axis) vertex.uv[axis] = static_cast<float>(uvs.read(index, axis));
            if (istangent)
            {
                MATH::VECTOR3 tangent{ static_cast<float>(tangents.read(index, 0)),
                    static_cast<float>(tangents.read(index, 1)), static_cast<float>(tangents.read(index, 2)) };
                write(vertex.tangent, direction(world, tangent));
                vertex.tangent[3] = static_cast<float>(tangents.read(index, 3)) * (determinant < 0 ? -1.0f : 1.0f);
            }
        }
        if (primitive.HasKey(L"indices"))
        {
            const auto index_accessor = accessor(json, number(primitive, L"indices"), buffers, L"SCALAR");
            require(index_accessor.component_type != 5126 && !index_accessor.isnormalized && index_accessor.count <= 12000000,
                "Indices must be unsigned integers, with at most 12 million indices.");
            for (size_t index = 0; index < index_accessor.count; ++index)
            {
                const auto value = static_cast<uint32_t>(index_accessor.read(index, 0));
                require(value < vertices.size(), "Index references a missing vertex.");
                indices.push_back(value);
            }
        }
        else
            for (uint32_t index = 0; index < vertices.size(); ++index) indices.push_back(index);
        require(!indices.empty() && indices.size() % 3 == 0, "Invalid TRIANGLES index count.");
        if (determinant < 0)
            for (size_t index = 0; index < indices.size(); index += 3) std::swap(indices[index+1], indices[index+2]);

        if (!isnormal)
        {
            // glTF requires flat normals when NORMAL is absent; split shared corners first.
            require(indices.size() <= 4000000, "Generated flat normals exceed the vertex limit.");
            std::vector<GLTF_VERTEX> flatvertices;
            flatvertices.reserve(indices.size());
            for (uint32_t& index : indices)
            {
                flatvertices.push_back(vertices[index]);
                index = static_cast<uint32_t>(flatvertices.size() - 1);
            }
            vertices = std::move(flatvertices);
        }

        std::vector<MATH::VECTOR3> bitangents(vertices.size());
        for (size_t index = 0; index < indices.size(); index += 3)
        {
            const auto& v0 = vertices[indices[index]];
            const auto& v1 = vertices[indices[index+1]];
            const auto& v2 = vertices[indices[index+2]];
            const auto edge1 = MATH::subtract(vector(v1.position), vector(v0.position));
            const auto edge2 = MATH::subtract(vector(v2.position), vector(v0.position));
            const auto normal = MATH::cross(edge1, edge2);
            const float u1 = v1.uv[0]-v0.uv[0], v_1 = v1.uv[1]-v0.uv[1];
            const float u2 = v2.uv[0]-v0.uv[0], v_2 = v2.uv[1]-v0.uv[1];
            const float uv_determinant = u1*v_2-u2*v_1;
            for (size_t corner = 0; corner < 3; ++corner)
            {
                const size_t vertex_index = indices[index+corner];
                auto& vertex = vertices[vertex_index];
                if (!isnormal) write(vertex.normal, MATH::add(vector(vertex.normal), normal));
                if (!istangent && std::abs(uv_determinant) > 1e-8f)
                {
                    const auto tangent = MATH::multiply(MATH::subtract(MATH::multiply(edge1, v_2),
                        MATH::multiply(edge2, v_1)), 1 / uv_determinant);
                    const auto bitangent = MATH::multiply(MATH::subtract(MATH::multiply(edge2, u1),
                        MATH::multiply(edge1, u2)), 1 / uv_determinant);
                    write(vertex.tangent, MATH::add(vector(vertex.tangent), tangent));
                    bitangents[vertex_index] = MATH::add(bitangents[vertex_index], bitangent);
                }
            }
        }
        for (size_t index = 0; index < vertices.size(); ++index)
        {
            auto& vertex = vertices[index];
            auto normal = MATH::normalize(vector(vertex.normal));
            if (MATH::length(normal) < 0.5f) normal = { 0, 1, 0 };
            write(vertex.normal, normal);
            auto tangent = MATH::subtract(vector(vertex.tangent),
                MATH::multiply(normal, MATH::dot(normal, vector(vertex.tangent))));
            if (MATH::length(tangent) < 1e-6f)
                tangent = MATH::cross(std::abs(normal.y) < 0.9f ? MATH::VECTOR3{ 0,1,0 } : MATH::VECTOR3{ 1,0,0 }, normal);
            tangent = MATH::normalize(tangent);
            write(vertex.tangent, tangent);
            if (!istangent) vertex.tangent[3] = MATH::dot(MATH::cross(normal, tangent), bitangents[index]) < 0 ? -1.0f : 1.0f;
            require(vertex.tangent[3] == -1 || vertex.tangent[3] == 1, "Invalid tangent handedness.");
            for (float value : vertex.normal) require(std::isfinite(value), "Invalid transformed normal.");
            for (float value : vertex.tangent) require(std::isfinite(value), "Invalid transformed tangent.");
        }

    }
}

bool GLTF_MESH::load(const std::filesystem::path& file_path)
{
    _vertices.clear();
    _indices.clear();
    _materials.clear();
    _primitives.clear();
    _error.clear();
    try
    {
        auto extension = file_path.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
        require(extension == L".gltf", "Select a .gltf file. GLB is not supported yet.");
        const auto bytes = read_file(file_path, 32 * 1024 * 1024);
        const auto json = JsonObject::Parse(winrt::to_hstring(std::string(bytes.begin(), bytes.end())));
        require(json.GetNamedObject(L"asset").GetNamedString(L"version") == L"2.0", "Only glTF 2.0 is supported.");
        require(!json.HasKey(L"extensionsRequired") || json.GetNamedArray(L"extensionsRequired").Size() == 0,
            "This asset requires an unsupported glTF extension.");
        require(!json.HasKey(L"animations") || json.GetNamedArray(L"animations").Size() == 0,
            "Animated models are not supported yet.");
        std::vector<std::vector<uint8_t>> buffers;
        size_t total_bytes = 0;
        for (const auto& value : json.GetNamedArray(L"buffers"))
        {
            const auto item = value.GetObject();
            const size_t declared = number(item, L"byteLength");
            auto buffer = read_file(uri_path(file_path.parent_path(), item.GetNamedString(L"uri")),
                512 * 1024 * 1024 - total_bytes);
            total_bytes += buffer.size();
            require(declared > 0 && declared <= buffer.size(), "Buffer byteLength exceeds its file.");
            buffer.resize(declared);
            buffers.push_back(std::move(buffer));
        }

        const auto nodes = json.GetNamedArray(L"nodes");
        require(nodes.Size() <= 65536, "Too many glTF nodes.");
        std::vector<size_t> parents(nodes.Size(), SIZE_MAX);
        for (size_t index = 0; index < nodes.Size(); ++index)
        {
            const auto node = object_at(nodes, index);
            if (!node.HasKey(L"children")) continue;
            for (const auto& child : node.GetNamedArray(L"children"))
            {
                const size_t child_index = integer(child.GetNumber());
                require(child_index < nodes.Size() && parents[child_index] == SIZE_MAX && child_index != index,
                    "Invalid node hierarchy.");
                parents[child_index] = index;
            }
        }
        // Validate even unused hierarchies without recursive stack growth.
        std::vector<uint8_t> state(nodes.Size(), 0);
        for (size_t index = 0; index < nodes.Size(); ++index)
        {
            size_t current = index;
            std::vector<size_t> chain;
            while (current != SIZE_MAX && state[current] != 2)
            {
                require(state[current] != 1, "Cycle in node hierarchy.");
                state[current] = 1;
                chain.push_back(current);
                current = parents[current];
            }
            for (size_t node : chain) state[node] = 2;
        }
        struct NODE_WORK { size_t index; MATH::MATRIX4X4 parent; };
        std::vector<NODE_WORK> work;
        if (json.HasKey(L"scenes"))
        {
            const auto scene = object_at(json.GetNamedArray(L"scenes"), number(json, L"scene"));
            for (const auto& root : scene.GetNamedArray(L"nodes"))
            {
                const size_t index = integer(root.GetNumber());
                require(index < nodes.Size() && parents[index] == SIZE_MAX, "Invalid scene root.");
                work.push_back({ index, MATH::identity_matrix() });
            }
        }
        else
            for (size_t index = 0; index < nodes.Size(); ++index)
                if (parents[index] == SIZE_MAX) work.push_back({ index, MATH::identity_matrix() });
        std::reverse(work.begin(), work.end());
        std::vector<bool> visited(nodes.Size(), false);
        std::map<size_t, UINT> material_map;
        std::map<std::pair<size_t, size_t>, GLTF_PRIMITIVE> geometry_map;
        MATH::VECTOR3 minimum{ FLT_MAX, FLT_MAX, FLT_MAX }, maximum{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
        while (!work.empty())
        {
            const auto current = work.back();
            work.pop_back();
            require(!visited[current.index], "Duplicate node in active scene.");
            visited[current.index] = true;
            const auto node = object_at(nodes, current.index);
            require(!node.HasKey(L"skin") && !node.HasKey(L"extensions"), "Skin/node extensions are not supported.");
            const auto world = MATH::multiply(node_matrix(node), current.parent);
            if (node.HasKey(L"children"))
            {
                const auto children = node.GetNamedArray(L"children");
                for (size_t index = children.Size(); index > 0; --index)
                    work.push_back({ integer(children.GetNumberAt(static_cast<uint32_t>(index - 1))), world });
            }
            if (!node.HasKey(L"mesh")) continue;
            const size_t mesh_index = number(node, L"mesh");
            const auto mesh = object_at(json.GetNamedArray(L"meshes"), mesh_index);
            const auto primitives = mesh.GetNamedArray(L"primitives");
            require(primitives.Size() > 0, "Mesh has no primitives.");
            for (size_t primitive_index = 0; primitive_index < primitives.Size(); ++primitive_index)
            {
                require(_primitives.size() < 65536, "Too many mesh primitives.");
                const auto primitive = object_at(primitives, primitive_index);
                require(number(primitive, L"mode", 4) == 4 && !primitive.HasKey(L"targets") &&
                    !primitive.HasKey(L"extensions"), "Only uncompressed static TRIANGLES are supported.");
                const size_t material_key = number(primitive, L"material", SIZE_MAX);
                if (!material_map.contains(material_key))
                {
                    require(_materials.size() < 4096, "Too many materials.");
                    GLTF_MATERIAL material;
                    read_material(json, primitive, file_path.parent_path(), material);
                    material_map.emplace(material_key, static_cast<UINT>(_materials.size()));
                    _materials.push_back(std::move(material));
                }
                const auto key = std::make_pair(mesh_index, primitive_index);
                auto found = geometry_map.find(key);
                if (found == geometry_map.end())
                {
                    std::vector<GLTF_VERTEX> vertices;
                    std::vector<uint32_t> indices;
                    read_geometry(json, primitive, buffers, _materials[material_map.at(material_key)], vertices, indices);
                    require(_vertices.size() + vertices.size() <= 4000000 &&
                        _indices.size() + indices.size() <= 12000000, "Model exceeds geometry limits.");
                    GLTF_PRIMITIVE range;
                    range.first_vertex = static_cast<UINT>(_vertices.size());
                    range.vertex_count = static_cast<UINT>(vertices.size());
                    range.first_index = static_cast<UINT>(_indices.size());
                    range.index_count = static_cast<UINT>(indices.size());
                    range.material_index = material_map.at(material_key);
                    range.local_minimum = range.local_maximum = vector(vertices[0].position);
                    for (const auto& vertex : vertices)
                    {
                        auto& low = range.local_minimum;
                        auto& high = range.local_maximum;
                        low.x = (std::min)(low.x, vertex.position[0]); high.x = (std::max)(high.x, vertex.position[0]);
                        low.y = (std::min)(low.y, vertex.position[1]); high.y = (std::max)(high.y, vertex.position[1]);
                        low.z = (std::min)(low.z, vertex.position[2]); high.z = (std::max)(high.z, vertex.position[2]);
                    }
                    _vertices.insert(_vertices.end(), vertices.begin(), vertices.end());
                    for (uint32_t index : indices) _indices.push_back(index + range.first_vertex);
                    found = geometry_map.emplace(key, range).first;
                }
                auto draw = found->second;
                // Local vertices have reflected Z, so conjugate the node transform: S * M * S.
                draw.node_transform = world;
                for (size_t axis = 0; axis < 4; ++axis)
                {
                    draw.node_transform.values[2][axis] = -draw.node_transform.values[2][axis];
                    draw.node_transform.values[axis][2] = -draw.node_transform.values[axis][2];
                }
                const float determinant = MATH::dot(vector(draw.node_transform.values[0]),
                    MATH::cross(vector(draw.node_transform.values[1]), vector(draw.node_transform.values[2])));
                require(std::isfinite(determinant) && std::abs(determinant) > 1e-12f, "Singular node transform.");
                for (UINT corner = 0; corner < 8; ++corner)
                {
                    const MATH::VECTOR3 local{
                        corner & 1 ? draw.local_maximum.x : draw.local_minimum.x,
                        corner & 2 ? draw.local_maximum.y : draw.local_minimum.y,
                        corner & 4 ? draw.local_maximum.z : draw.local_minimum.z };
                    const auto point = MATH::transform_point(local, draw.node_transform);
                    require(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z),
                        "Invalid transformed bounds.");
                    minimum.x = (std::min)(minimum.x, point.x); maximum.x = (std::max)(maximum.x, point.x);
                    minimum.y = (std::min)(minimum.y, point.y); maximum.y = (std::max)(maximum.y, point.y);
                    minimum.z = (std::min)(minimum.z, point.z); maximum.z = (std::max)(maximum.z, point.z);
                }
                _primitives.push_back(draw);
            }
        }
        require(!_primitives.empty(), "The active scene contains no renderable mesh.");
        _center = MATH::multiply(MATH::add(minimum, maximum), 0.5f);
        _radius = MATH::length(MATH::subtract(maximum, minimum)) * 0.5f;
        require(std::isfinite(_radius) && _radius > 1e-6f && _radius < 1e6f, "Invalid model bounds.");
        return true;
    }
    catch (const winrt::hresult_error&)
    {
        _error = "Invalid glTF JSON, missing required fields, or Windows JSON parser failure.";
    }
    catch (const std::exception& error) { _error = error.what(); }
    _vertices.clear();
    _indices.clear();
    _materials.clear();
    _primitives.clear();
    return false;
}

const std::vector<GLTF_VERTEX>& GLTF_MESH::get_vertices() const { return _vertices; }
const std::vector<uint32_t>& GLTF_MESH::get_indices() const { return _indices; }
size_t GLTF_MESH::get_triangle_count() const
{
    size_t count = 0;
    for (const auto& primitive : _primitives) count += primitive.index_count / 3;
    return count;
}
