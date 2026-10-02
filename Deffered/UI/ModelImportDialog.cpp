#include "../Common/stdafx.h"
#include "ModelImportDialog.h"
#include "../ThirdParty/ImGui/imgui.h"

void MODEL_IMPORT_DIALOG::open(VIEWER_REQUEST request)
{
    _pending = std::move(request);
    _ispending = true;
    _isopening = true;
}

void MODEL_IMPORT_DIALOG::draw()
{
    if (_isopening)
    {
        ImGui::OpenPopup("Import Model");
        _isopening = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 460, 0 }, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Import Model", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    const auto filename = _pending.path.filename().u8string();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 428);
    ImGui::TextUnformatted(reinterpret_cast<const char*>(filename.c_str()));
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    if (ImGui::BeginTable("Transform", 4, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn("Transform");
        ImGui::TableSetupColumn("X");
        ImGui::TableSetupColumn("Y");
        ImGui::TableSetupColumn("Z");
        ImGui::TableHeadersRow();
        const char* labels[] = { "Position", "Rotation (deg)", "Scale" };
        MATH::VECTOR3* vectors[] = { &_pending.position, &_pending.rotation, &_pending.scale };
        for (int row = 0; row < 3; ++row)
        {
            ImGui::PushID(row);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(labels[row]);
            float* components[] = { &vectors[row]->x, &vectors[row]->y, &vectors[row]->z };
            for (int column = 0; column < 3; ++column)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(column);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputFloat("##value", components[column], 0, 0, "%.4g");
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    const bool isvalid = _pending.has_valid_transform();
    if (!isvalid) ImGui::TextColored({ 1, 0.5f, 0.3f, 1 }, "Invalid transform. Scale range: 0.0001 - 10000.");
    ImGui::Separator();
    if (ImGui::Button("RESET", { 100, 28 }))
    {
        _pending.position = {};
        _pending.rotation = {};
        _pending.scale = { 1, 1, 1 };
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!isvalid);
    const bool isaccepted = ImGui::Button("IMPORT", { 150, 28 });
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool iscancelled = ImGui::Button("CANCEL", { 150, 28 }) || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (isaccepted || iscancelled)
    {
        if (isaccepted) _ready = std::move(_pending);
        _pending = {};
        _ispending = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

VIEWER_REQUEST MODEL_IMPORT_DIALOG::take_request()
{
    auto request = std::move(_ready);
    _ready = {};
    return request;
}
