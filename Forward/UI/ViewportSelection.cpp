#include "../Common/stdafx.h"
#include "ViewportSelection.h"
#include "../Scene/ScenePicker.h"
#include "../Renderer/Manager/CameraManager.h"
#include "../Renderer/Manager/InputManager.h"
#include "../ThirdParty/ImGui/imgui.h"

void VIEWPORT_SELECTION::update(SCENE& scene)
{
    const auto& io = ImGui::GetIO();
    if (io.WantCaptureMouse || ImGui::IsAnyItemActive() ||
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
        INPUT_MANAGER::get_instance().is_right_mouse_down() ||
        !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    const auto size = io.DisplaySize;
    const auto point = io.MousePos;
    if (size.x <= 0 || size.y <= 0 || point.x < 0 || point.y < 0 || point.x >= size.x || point.y >= size.y) return;
    MATH::VECTOR3 origin, direction;
    CAMERA_MANAGER::get_instance().get_pick_ray(2 * point.x / size.x - 1,
        1 - 2 * point.y / size.y, size.x / size.y, origin, direction);
    scene.select(SCENE_PICKER::pick(scene, origin, direction));
}
