#include "../Deffered/Resource/GltfMesh.h"
#include "../Deffered/Scene/Scene.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <roapi.h>
#include <iostream>
#include <stdexcept>

using winrt::Windows::Data::Json::JsonObject;
using winrt::Windows::Data::Json::JsonValue;

namespace
{
    void check(bool isvalid, const char* message)
    {
        if (!isvalid) throw std::runtime_error(message);
    }

    struct FIXTURE
    {
        std::filesystem::path directory;
        FIXTURE()
        {
            directory = std::filesystem::temp_directory_path() /
                (L"d3d12-import-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
            check(std::filesystem::create_directory(directory), "Could not create isolated fixture directory.");
        }
        ~FIXTURE()
        {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
        void binary(const std::vector<uint8_t>& bytes)
        {
            std::ofstream file(directory / L"mesh data.bin", std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            check(static_cast<bool>(file), "Could not write fixture buffer.");
        }
        std::filesystem::path json(const JsonObject& object)
        {
            // A non-ASCII filename exercises the filesystem path without depending on source encoding.
            const auto path = directory / L"\uBAA8\uB378.gltf";
            std::ofstream file(path, std::ios::binary);
            file << winrt::to_string(object.Stringify());
            file.close();
            check(static_cast<bool>(file), "Could not write fixture JSON.");
            return path;
        }
    };

    JsonObject document()
    {
        return JsonObject::Parse(LR"({
          "asset":{"version":"2.0"},
          "buffers":[{"uri":"mesh%20data.bin","byteLength":72}],
          "bufferViews":[{"buffer":0,"byteOffset":8,"byteLength":48,"byteStride":16},
                         {"buffer":0,"byteOffset":56,"byteLength":16}],
          "accessors":[{"bufferView":0,"byteOffset":4,"componentType":5126,"count":3,"type":"VEC3"},
                       {"bufferView":1,"byteOffset":4,"componentType":5125,"count":3,"type":"SCALAR"}],
          "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":1}]}],
          "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0,1,0,1]}},
                       {"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1],"metallicFactor":0}}],
          "nodes":[{"mesh":0,"translation":[10,2,3],"scale":[2,1,1]}],
          "scenes":[{"nodes":[0]}],"scene":0
        })");
    }

    std::vector<uint8_t> binary()
    {
        std::vector<uint8_t> bytes(72);
        const float positions[3][3]{ {0,0,0}, {1,0,0}, {0,1,0} };
        for (size_t index = 0; index < 3; ++index)
            std::memcpy(bytes.data() + 12 + index * 16, positions[index], 12);
        const uint32_t indices[3]{0,1,2};
        std::memcpy(bytes.data() + 60, indices, sizeof(indices));
        return bytes;
    }

    void expect_failure(FIXTURE& fixture, const JsonObject& json, const char* message)
    {
        GLTF_MESH mesh;
        check(!mesh.load(fixture.json(json)) && !mesh.get_error().empty() && mesh.get_vertices().empty(), message);
    }
}

int main()
{
    const HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialized)) return 1;
    int result = 0;
    try
    {
        FIXTURE fixture;
        fixture.binary(binary());
        auto json = document();
        GLTF_MESH mesh;
        check(mesh.load(fixture.json(json)), "Valid interleaved glTF failed to load.");
        check(mesh.get_vertices().size() == 3 && mesh.get_indices().size() == 3, "Incorrect vertex/index count.");
        check(std::abs(mesh.get_center().x - 11) < 0.001f &&
            std::abs(mesh.get_center().y - 2.5f) < 0.001f &&
            std::abs(mesh.get_center().z + 3) < 0.001f, "Node transform or RH to LH conversion failed.");
        check(mesh.get_material().base_color_factor[0] == 1 &&
            mesh.get_material().base_color_factor[1] == 0, "Primitive material index was ignored.");
        check(mesh.get_material().texture_paths[0].empty(), "Missing texture must request a fallback.");
        for (const auto& vertex : mesh.get_vertices())
            check(std::abs(vertex.normal[2] + 1) < 0.001f && std::isfinite(vertex.tangent[0]),
                "Generated normal/tangent is invalid.");

        for (int component : { 5121, 5123, 5125 })
        {
            auto bytes = binary();
            const size_t width = component == 5121 ? 1 : component == 5123 ? 2 : 4;
            for (uint32_t index = 0; index < 3; ++index) std::memcpy(bytes.data()+60+index*width, &index, width);
            fixture.binary(bytes);
            auto typed = document();
            typed.GetNamedArray(L"accessors").GetObjectAt(1).Insert(L"componentType", JsonValue::CreateNumberValue(component));
            check(mesh.load(fixture.json(typed)), "Unsigned index component type failed.");
        }
        fixture.binary(binary());
        json = document();
        auto textured_bytes = binary();
        textured_bytes.resize(78);
        textured_bytes[74] = 255;
        textured_bytes[77] = 255;
        fixture.binary(textured_bytes);
        json.GetNamedArray(L"buffers").GetObjectAt(0).Insert(L"byteLength", JsonValue::CreateNumberValue(78));
        json.GetNamedArray(L"bufferViews").Append(JsonObject::Parse(LR"({"buffer":0,"byteOffset":72,"byteLength":6})"));
        json.GetNamedArray(L"accessors").Append(JsonObject::Parse(LR"({"bufferView":2,"componentType":5121,"normalized":true,"count":3,"type":"VEC2"})"));
        json.GetNamedArray(L"meshes").GetObjectAt(0).GetNamedArray(L"primitives").GetObjectAt(0)
            .GetNamedObject(L"attributes").Insert(L"TEXCOORD_0", JsonValue::CreateNumberValue(2));
        const auto image_data = JsonObject::Parse(LR"({"images":[{"uri":"ignored.png"},{"uri":"base%20color.png"}],"textures":[{"source":1}]})");
        json.Insert(L"images", image_data.GetNamedArray(L"images"));
        json.Insert(L"textures", image_data.GetNamedArray(L"textures"));
        json.GetNamedArray(L"materials").GetObjectAt(1).GetNamedObject(L"pbrMetallicRoughness")
            .Insert(L"baseColorTexture", JsonObject::Parse(LR"({"index":0})"));
        check(mesh.load(fixture.json(json)), "Normalized UV or texture indirection failed.");
        check(mesh.get_material().texture_paths[0].filename() == L"base color.png", "Texture/image reference was ignored.");
        check(mesh.get_vertices()[1].uv[1] == 1, "Normalized UV or winding conversion is incorrect.");
        fixture.binary(binary());
        json = document();
        json.GetNamedArray(L"accessors").GetObjectAt(0).Insert(L"count", JsonValue::CreateNumberValue(4000000000.0));
        expect_failure(fixture, json, "Oversized accessor was accepted.");
        json = document();
        json.GetNamedArray(L"accessors").GetObjectAt(0).Insert(L"byteOffset", JsonValue::CreateNumberValue(47));
        expect_failure(fixture, json, "Out-of-bounds accessor offset was accepted.");
        json = document();
        json.GetNamedArray(L"meshes").GetObjectAt(0).GetNamedArray(L"primitives").GetObjectAt(0)
            .Insert(L"mode", JsonValue::CreateNumberValue(1));
        expect_failure(fixture, json, "Non-triangle topology was accepted.");
        json = document();
        const auto primitives = json.GetNamedArray(L"meshes").GetObjectAt(0).GetNamedArray(L"primitives");
        auto second_primitive = JsonObject::Parse(primitives.GetObjectAt(0).Stringify());
        second_primitive.Insert(L"material", JsonValue::CreateNumberValue(0));
        primitives.Append(second_primitive);
        json.GetNamedArray(L"nodes").Append(JsonObject::Parse(LR"({"mesh":0,"translation":[-10,0,0],"scale":[-1,2,1]})"));
        json.GetNamedArray(L"scenes").GetObjectAt(0).GetNamedArray(L"nodes").Append(JsonValue::CreateNumberValue(1));
        check(mesh.load(fixture.json(json)), "Multi-node/multi-material model failed.");
        check(mesh.get_primitives().size() == 4 && mesh.get_materials().size() == 2,
            "Primitive/material mapping was lost.");
        check(mesh.get_vertices().size() == 6 && mesh.get_indices().size() == 6 && mesh.get_triangle_count() == 4,
            "Repeated mesh nodes must reuse geometry and preserve draw counts.");
        check(mesh.get_primitives()[0].material_index != mesh.get_primitives()[1].material_index,
            "Different materials were merged.");
        check(mesh.get_primitives()[0].first_index == mesh.get_primitives()[2].first_index,
            "Repeated node geometry was uploaded twice.");
        MATH::MATRIX4X4 normal;
        float sign = 0;
        check(MATH::normal_matrix(mesh.get_primitives()[2].node_transform, normal, sign) && sign == -1,
            "Mirrored node tangent sign is incorrect.");
        const auto world_point = MATH::transform_point({1,0,0}, mesh.get_primitives()[2].node_transform);
        check(std::abs(world_point.x + 11) < 0.001f, "Per-node world transform was lost.");
        json = document();
        json.GetNamedArray(L"nodes").Append(JsonObject::Parse(LR"({"translation":[0,0,5],"children":[0]})"));
        json.GetNamedArray(L"scenes").GetObjectAt(0).GetNamedArray(L"nodes").SetAt(0, JsonValue::CreateNumberValue(1));
        check(mesh.load(fixture.json(json)) && std::abs(mesh.get_center().z + 8) < 0.001f,
            "Parent node transform was not composed.");
        json.GetNamedArray(L"nodes").GetObjectAt(0).Insert(L"children",
            JsonObject::Parse(LR"({"children":[1]})").GetNamedArray(L"children"));
        expect_failure(fixture, json, "Cyclic node hierarchy was accepted.");
        auto bytes = binary();
        const uint32_t invalid_index = 99;
        std::memcpy(bytes.data()+60, &invalid_index, 4);
        fixture.binary(bytes);
        expect_failure(fixture, document(), "Invalid index was accepted.");
        fixture.binary(std::vector<uint8_t>(8));
        expect_failure(fixture, document(), "Truncated buffer was accepted.");
        RESOURCE_CACHE<MODEL_RESOURCE> cache;
        SCENE scene;
        auto shared_model = std::make_shared<MODEL_RESOURCE>();
        cache.insert(L"model", shared_model);
        const auto first = scene.add(shared_model);
        const auto second = scene.add(cache.find(L"model"));
        check(first != second && scene.find(first)->model == scene.find(second)->model,
            "Scene instances must share model resources with distinct IDs.");
        scene.find(first)->position.x = 7;
        scene.find(first)->materials.resize(1);
        scene.find(second)->materials.resize(1);
        scene.find(first)->materials[0].roughness_multiplier = 0.25f;
        check(scene.find(second)->position.x != 7 && scene.find(second)->materials[0].roughness_multiplier == 1,
            "Object overrides leaked into another instance.");
        scene.find(first)->isvisible = false;
        check(scene.visible_count() == 1, "Hidden objects were counted as visible.");
        shared_model.reset();
        scene.erase(first);
        check(cache.find(L"model") != nullptr && scene.selected()->id == second,
            "Deleting an instance released a still-shared resource.");
        scene.erase(second);
        cache.prune();
        check(cache.live_count() == 0 && !scene.selected(), "Last deletion must release the resource and selection.");
        auto replacement = std::make_shared<MODEL_RESOURCE>();
        const auto third = scene.add(replacement);
        check(third > second && !scene.replace(first, replacement), "Scene IDs were reused or stale IDs accepted.");
        scene.find(third)->position.x = 12;
        auto different = std::make_shared<MODEL_RESOURCE>();
        check(scene.replace(third, different) && scene.find(third)->position.x == 12 &&
            scene.find(third)->model == different, "OPEN replacement must preserve the object transform.");
        for (size_t index = scene.get_objects().size(); index < SCENE::object_limit; ++index) scene.add(different);
        bool islimited = false;
        try { scene.add(different); } catch (const std::runtime_error&) { islimited = true; }
        check(islimited && scene.get_objects().size() == SCENE::object_limit, "Object limit changed the active scene.");
        std::cout << "glTF import, scene, and weak-cache CPU regression tests passed.\n";
    }
    catch (const winrt::hresult_error& error)
    {
        std::cerr << winrt::to_string(error.message()) << '\n';
        result = 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RoUninitialize();
    return result;
}
