#include "../Common/stdafx.h"
#include "ViewerPanels.h"
#include "../ThirdParty/ImGui/imgui.h"

namespace
{
    bool button(const char* label, bool isactive, ImVec2 size)
    {
        if (isactive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.30f, 0.06f, 1.0f));
        const bool ispressed = ImGui::Button(label, size);
        if (isactive) ImGui::PopStyleColor();
        return ispressed;
    }

    bool begin_panel(const char* title, bool& isopen, ImVec2 position, ImVec2 size)
    {
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        const bool isvisible = ImGui::Begin(title, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        if (!isvisible) return false;
        ImGui::TextUnformatted(title);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 36.0f);
        if (ImGui::SmallButton("X")) isopen = false;
        ImGui::Separator();
        return true;
    }

    void toggle(const char* label, bool& isopen, bool& ishidden)
    {
        if (button(label, isopen && !ishidden, { 78.0f, 28.0f }))
        {
            if (ishidden) { ishidden = false; isopen = true; }
            else isopen = !isopen;
        }
    }

    void slider_with_value(const char* label, float* value, float min, float max,
        const char* format)
    {
        const float row_start = ImGui::GetCursorPosX();
        const float total_width = ImGui::CalcItemWidth();
        const float value_width = ImGui::CalcTextSize("-5.00 EV").x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::AlignTextToFramePadding();
        ImGui::Text(format, *value);
        ImGui::SameLine(row_start + value_width + spacing);
        ImGui::SetNextItemWidth(total_width - value_width - spacing);
        ImGui::SliderFloat(label, value, min, max, "", ImGuiSliderFlags_NoInput);
    }

    void axis_inputs(MATH::VECTOR3& value, float speed, float min, float max)
    {
        if (ImGui::BeginTable("axes", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("axis", ImGuiTableColumnFlags_WidthFixed, 18.0f);
            ImGui::TableSetupColumn("value");
            const char* labels[] = { "X", "Y", "Z" };
            float* values[] = { &value.x, &value.y, &value.z };
            for (int index = 0; index < 3; ++index)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(labels[index]);
                ImGui::TableNextColumn();
                ImGui::PushID(index);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##axis", values[index], speed, min, max, "%.3f",
                    ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
}

void VIEWER_PANELS::draw(VIEWER_SETTINGS& settings, const VIEWER_STATS& stats,
    const std::array<UINT64, 5>& textures)
{
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const bool iscompact = screen.x < 1050.0f;
    const float toolbar_height = iscompact ? 82.0f : 46.0f;
    const float status_height = _is_status && !_is_hidden ? 28.0f : 0.0f;
    const float top = toolbar_height + 8.0f;
    const float bottom = screen.y - status_height - 8.0f;
    const float available = (std::max)(bottom - top, 200.0f);
    const float left_width = 236.0f;
    const float right_width = 292.0f;
    const float right = screen.x - right_width - 8.0f;
    const auto fixed_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ screen.x, toolbar_height });
    ImGui::Begin("Toolbar", nullptr, fixed_flags | ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginDisabled();
    ImGui::Button("OPEN MODEL", { 110, 28 });
    ImGui::SameLine();
    ImGui::Button("ADD MODEL", { 110, 28 });
    ImGui::EndDisabled();
    if (!iscompact)
    {
        ImGui::SameLine();
        ImGui::SetCursorPosX((std::max)(240.0f, screen.x - 602.0f));
    }
    toggle("SCENE", _is_scene, _is_hidden);
    ImGui::SameLine(); toggle("INSPECTOR", _is_inspector, _is_hidden);
    ImGui::SameLine(); toggle("DISPLAY", _is_display, _is_hidden);
    ImGui::SameLine(); toggle("LIGHT", _is_light, _is_hidden);
    ImGui::SameLine(); toggle("ASSETS", _is_assets, _is_hidden);
    ImGui::SameLine(); toggle("STATUS", _is_status, _is_hidden);
    ImGui::SameLine();
    if (button(_is_hidden ? "RESTORE" : "HIDE ALL", _is_hidden, { 78, 28 }))
        _is_hidden = !_is_hidden;
    ImGui::End();

    if (_is_hidden) return;
    if (_is_scene)
    {
        if (begin_panel("SCENE", _is_scene, { 8, top }, { left_width, available * 0.34f }))
        {
            if (ImGui::Selectable("DamagedHelmet", _is_selected)) _is_selected = true;
            ImGui::Checkbox("Visible", &settings.is_mesh_visible);
            if (ImGui::Button("SELECT", { 68, 28 })) _is_selected = true;
            ImGui::SameLine();
            if (ImGui::Button(settings.is_mesh_visible ? "HIDE" : "SHOW", { 68, 28 }))
                settings.is_mesh_visible = !settings.is_mesh_visible;
            ImGui::SameLine();
            ImGui::BeginDisabled();
            ImGui::Button("DELETE", { 64, 28 });
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    const float inspector_height = (available - 16.0f) * 0.38f;
    const float display_height = (available - 16.0f) * 0.28f;
    const float light_height = available - inspector_height - display_height - 16.0f;
    if (_is_inspector)
    {
        if (begin_panel("INSPECTOR", _is_inspector, { right, top },
            { right_width, inspector_height }))
        {
            ImGui::TextUnformatted("DamagedHelmet");
            if (button("MESH", !_is_material_tab, { 124, 25 })) _is_material_tab = false;
            ImGui::SameLine();
            if (button("MATERIAL", _is_material_tab, { 124, 25 })) _is_material_tab = true;
            if (!_is_material_tab)
            {
                const char* tabs[] = { "POSITION", "ROTATION", "SCALE" };
                const float tab_width = (ImGui::GetContentRegionAvail().x - 12.0f) / 3.0f;
                for (int index = 0; index < 3; ++index)
                {
                    if (index) ImGui::SameLine();
                    if (button(tabs[index], _transform_tab == index, { tab_width, 25 }))
                        _transform_tab = index;
                }
                if (_transform_tab == 0) axis_inputs(settings.position, 0.02f, -100.0f, 100.0f);
                if (_transform_tab == 1) axis_inputs(settings.rotation, 0.5f, -180.0f, 180.0f);
                if (_transform_tab == 2) axis_inputs(settings.scale, 0.01f, 0.01f, 10.0f);
                if (ImGui::Button("RESET TRANSFORM", { -1, 25 }))
                {
                    settings.position = {};
                    settings.rotation = {};
                    settings.scale = { 1, 1, 1 };
                }
            }
            else
            {
                ImGui::ColorEdit4("Tint", &settings.base_color.x, ImGuiColorEditFlags_NoInputs);
                ImGui::SliderFloat("Metallic", &settings.metallic_multiplier, 0, 1);
                ImGui::SliderFloat("Roughness", &settings.roughness_multiplier, 0, 1);
                ImGui::SliderFloat("Emissive", &settings.emissive_multiplier, 0, 5);
            }
        }
        ImGui::End();
    }
    if (_is_display)
    {
        if (begin_panel("DISPLAY", _is_display, { right, top + inspector_height + 8 },
            { right_width, display_height }))
        {
            const char* modes[] = { "LIT", "BASE COLOR", "NORMAL", "METALLIC",
                "ROUGHNESS", "AO", "EMISSIVE", "DEPTH", "WIREFRAME" };
            const float mode_width = (ImGui::GetContentRegionAvail().x - 12.0f) / 3.0f;
            for (int index = 0; index < 9; ++index)
            {
                if (index % 3) ImGui::SameLine();
                if (button(modes[index], static_cast<int>(settings.mode) == index, { mode_width, 28 }))
                    settings.mode = static_cast<VIEW_MODE>(index);
            }
            ImGui::Checkbox("Skybox", &settings.is_skybox_visible);
            if (settings.mode == VIEW_MODE::depth)
                ImGui::SliderFloat("Range", &settings.depth_range, 1, 100, "%.1f");
        }
        ImGui::End();
    }
    if (_is_light)
    {
        if (begin_panel("LIGHT", _is_light,
            { right, top + inspector_height + display_height + 16 }, { right_width, light_height }))
        {
            ImGui::PushItemWidth(160.0f);
            ImGui::DragFloat3("Direction", &settings.light_direction.x, 0.01f, -1, 1, "%.2f");
            ImGui::ColorEdit3("Color", &settings.light_color.x, ImGuiColorEditFlags_NoInputs);
            slider_with_value("Intensity", &settings.light_intensity, 0, 20, "%.2f");
            slider_with_value("Environment", &settings.environment_intensity, 0, 3, "%.2f");
            slider_with_value("Exposure", &settings.exposure, -5, 5, "%.2f EV");
            ImGui::Checkbox("Rotate light", &settings.is_light_orbiting);
            ImGui::PopItemWidth();
        }
        ImGui::End();
    }
    if (_is_assets)
    {
        const float height = (std::min)(230.0f, available * 0.42f);
        if (begin_panel("ASSETS", _is_assets, { 8, bottom - height }, { left_width, height }))
        {
            if (button("MODELS", !_is_texture_tab, { 101, 25 })) _is_texture_tab = false;
            ImGui::SameLine();
            if (button("TEXTURES", _is_texture_tab, { 101, 25 })) _is_texture_tab = true;
            if (_is_texture_tab)
            {
                const char* labels[] = { "Base Color", "Normal", "Metallic-Roughness", "AO", "Emissive" };
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##texture", &_texture_index, labels, 5);
                const float side = (std::max)(40.0f, (std::min)(
                    ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y));
                if (textures[_texture_index])
                    ImGui::Image(static_cast<ImTextureID>(textures[_texture_index]), { side, side });
            }
            else
            {
                if (ImGui::Selectable("DamagedHelmet.gltf", _is_selected))
                {
                    _is_selected = true;
                    _is_inspector = true;
                }
                ImGui::Text("Vertices: %u", stats.vertex_count);
                ImGui::Text("Triangles: %u", stats.triangle_count);
            }
        }
        ImGui::End();
    }
    if (_is_status)
    {
        ImGui::SetNextWindowPos({ 0, screen.y - 28 });
        ImGui::SetNextWindowSize({ screen.x, 28 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 5));
        ImGui::Begin("Status", nullptr, fixed_flags | ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("%.0f FPS     Objects: %u     Triangles: %u",
            ImGui::GetIO().Framerate, settings.is_mesh_visible ? 1u : 0u,
            settings.is_mesh_visible ? stats.triangle_count : 0u);
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
