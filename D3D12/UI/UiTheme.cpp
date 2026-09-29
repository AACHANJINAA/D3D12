#include "../Common/stdafx.h"
#include "UiTheme.h"
#include "../ThirdParty/ImGui/imgui.h"

void apply_viewer_theme()
{
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = { 12.0f, 10.0f };
    style.FramePadding = { 8.0f, 6.0f };
    style.ItemSpacing = { 6.0f, 6.0f };
    style.ScrollbarSize = 10.0f;
    auto* colors = style.Colors;
    colors[ImGuiCol_Text] = { 0.90f, 0.91f, 0.92f, 1.0f };
    colors[ImGuiCol_TextDisabled] = { 0.48f, 0.49f, 0.50f, 1.0f };
    colors[ImGuiCol_WindowBg] = { 0.12f, 0.13f, 0.14f, 0.98f };
    colors[ImGuiCol_Border] = { 0.27f, 0.28f, 0.29f, 1.0f };
    colors[ImGuiCol_FrameBg] = { 0.16f, 0.17f, 0.18f, 1.0f };
    colors[ImGuiCol_FrameBgHovered] = { 0.25f, 0.26f, 0.27f, 1.0f };
    colors[ImGuiCol_FrameBgActive] = { 0.31f, 0.32f, 0.33f, 1.0f };
    colors[ImGuiCol_Button] = { 0.22f, 0.23f, 0.25f, 1.0f };
    colors[ImGuiCol_ButtonHovered] = { 0.33f, 0.34f, 0.36f, 1.0f };
    colors[ImGuiCol_ButtonActive] = { 0.76f, 0.32f, 0.06f, 1.0f };
    colors[ImGuiCol_Header] = { 0.32f, 0.24f, 0.17f, 1.0f };
    colors[ImGuiCol_HeaderHovered] = { 0.45f, 0.28f, 0.13f, 1.0f };
    colors[ImGuiCol_HeaderActive] = { 0.65f, 0.29f, 0.06f, 1.0f };
    colors[ImGuiCol_CheckMark] = { 1.0f, 0.49f, 0.12f, 1.0f };
    colors[ImGuiCol_SliderGrab] = { 0.93f, 0.42f, 0.08f, 1.0f };
    colors[ImGuiCol_SliderGrabActive] = { 1.0f, 0.59f, 0.18f, 1.0f };
}
