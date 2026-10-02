// Run from the repository root after compiling with /std:c++20 /EHsc /Y-:
// cl Tests\ViewerInteractionTests.cpp Deffered\Resource\GltfMesh.cpp Deffered\Scene\Scene.cpp
//    Deffered\Scene\ScenePicker.cpp /Fe:Tests\ViewerInteractionTests.exe /link windowsapp.lib
#include "../Deffered/Scene/ScenePicker.h"
#include "../Deffered/UI/ViewerPanels.h"
#include <roapi.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void check(bool isvalid, const char* message)
    {
        if (!isvalid) throw std::runtime_error(message);
    }
}

int main()
{
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED))) return 1;
    int result = 0;
    try
    {
        VIEWER_REQUEST request;
        check(request.has_valid_transform(), "Default import transform rejected.");
        request.scale.x = 0;
        check(!request.has_valid_transform(), "Zero scale accepted.");
        request.scale.x = -1;
        check(!request.has_valid_transform(), "Negative import scale accepted.");
        request.scale.x = 10001;
        check(!request.has_valid_transform(), "Excessive scale accepted.");
        request.scale.x = 1;
        request.position.y = (std::numeric_limits<float>::infinity)();
        check(!request.has_valid_transform(), "Infinite position accepted.");
        request.position.y = 0;
        request.rotation.z = (std::numeric_limits<float>::quiet_NaN)();
        check(!request.has_valid_transform(), "NaN rotation accepted.");

        GLTF_MESH mesh;
        check(mesh.load(L"Asset/Mesh/BoxTextured/glTF/BoxTextured.gltf"), "BoxTextured load failed.");
        const auto center = mesh.get_center();
        const auto radius = mesh.get_radius();
        float distance = 1000;
        const auto origin = MATH::add(center, { 0, 0, -radius * 3 });
        check(SCENE_PICKER::pick_mesh(mesh, MATH::identity_matrix(), origin, { 0, 0, 1 }, distance),
            "Center ray missed box.");
        check(distance > 0 && distance < radius * 3, "Invalid nearest distance.");
        distance = 1000;
        check(!SCENE_PICKER::pick_mesh(mesh, MATH::identity_matrix(), origin, { 0, 0, -1 }, distance),
            "Ray selected geometry behind the camera.");
        distance = 1000;
        check(!SCENE_PICKER::pick_mesh(mesh, MATH::identity_matrix(),
            MATH::add(origin, { radius * 5, 0, 0 }), { 0, 0, 1 }, distance), "Miss ray selected box.");
        const auto world = MATH::matrix_world({ 10, -2, 4 }, { 20, 40, 15 }, { -2, 3, 0.5f });
        const auto transformed_center = MATH::transform_point(center, world);
        const auto transformed_origin = MATH::add(transformed_center, { 0, 0, -20 });
        distance = 1000;
        check(SCENE_PICKER::pick_mesh(mesh, world, transformed_origin, { 0, 0, 1 }, distance),
            "Rotated, mirrored, nonuniformly scaled box was missed.");
        const float closest = distance;
        check(!SCENE_PICKER::pick_mesh(mesh, world, transformed_origin, { 0, 0, 1 }, distance),
            "Equal-distance hit replaced the first hit.");
        distance = closest * 0.5f;
        check(!SCENE_PICKER::pick_mesh(mesh, world, transformed_origin, { 0, 0, 1 }, distance),
            "Farther hit replaced a nearer object.");

        SCENE scene;
        auto empty_model = std::make_shared<MODEL_RESOURCE>();
        scene.add(empty_model);
        scene.select(0);
        check(!scene.selected(), "Clicking the background did not clear selection.");
        check(SCENE_PICKER::pick(scene, {}, { 0, 0, 1 }) == 0, "Empty geometry was selected.");
        for (const auto* name : { L"Duck", L"Avocado", L"BoomBox", L"MetalRoughSpheresNoTextures" })
        {
            const auto path = std::filesystem::path(L"Asset/Mesh") / name / L"glTF" / (std::wstring(name) + L".gltf");
            check(mesh.load(path), "Downloaded glTF model failed CPU import.");
            if (std::wstring(name) == L"MetalRoughSpheresNoTextures")
                check(mesh.get_materials().size() > 1 && mesh.get_primitives().size() > 1,
                    "Material/node mapping was lost.");
        }
        std::cout << "Import transform, picking and sample model checks passed.\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RoUninitialize();
    return result;
}
