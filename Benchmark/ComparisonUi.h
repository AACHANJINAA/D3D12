#pragma once
#include "BenchmarkLights.h"

inline void draw_comparison_ui(BENCHMARK_UI_STATE& state)
{
    const bool isforward = std::strcmp(state.renderer, "Forward") == 0;
    ImGui::SetNextWindowPos({12, 48}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({435, 0}, ImGuiCond_Always);
    ImGui::Begin("RENDERER LAB", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%s  |  %u x %u", state.renderer, state.width, state.height);
    ImGui::Text("Objects %llu   Lights %d   Culling OFF",
        static_cast<unsigned long long>(state.objects), state.light_count);
    const char* studies[] = {"1 CPU", "2 G-BUFFER", "3 FORWARD / DEFERRED"};
    ImGui::BeginDisabled(state.islocked);
    for (int index = 0; index < 3; ++index)
    {
        if (index) ImGui::SameLine();
        if (state.study == index) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.18f,.38f,.55f,1));
        const bool isselected = state.study == index;
        if (ImGui::Button(studies[index], {index == 0 ? 70.0f : (index == 1 ? 100.0f : 223.0f), 25}))
        { state.study = index; state.isreference = false; }
        if (isselected) ImGui::PopStyleColor();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(state.islocked || std::strcmp(state.profile, "matched_quality") != 0);
    if (state.study == 0)
    {
        ImGui::Checkbox("Instancing", &state.isinstancing); ImGui::SameLine();
        ImGui::BeginDisabled(!isforward);
        ImGui::Checkbox("SRV cache", &state.iscached);
        ImGui::EndDisabled();
    }
    else if (state.study == 1)
    {
        ImGui::BeginDisabled(isforward);
        if (ImGui::RadioButton("Full / 40 B", !state.iscompact)) state.iscompact = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Compact / 20 B", state.iscompact)) state.iscompact = true;
        ImGui::RadioButton("Lit", &state.view, 0); ImGui::SameLine();
        ImGui::RadioButton("Normals", &state.view, 2); ImGui::SameLine();
        ImGui::RadioButton("Depth", &state.view, 7);
        ImGui::EndDisabled();
    }
    else
    {
        if (ImGui::RadioButton("Low overlap", state.workload == 0))
        { state.workload = 0; state.isreference = false; }
        ImGui::SameLine();
        if (ImGui::RadioButton("20 layers / far-first", state.workload == 1))
        { state.workload = 1; state.isreference = false; }
        ImGui::Text("%3d", state.light_count); ImGui::SameLine();
        ImGui::SetNextItemWidth(245);
        if (ImGui::SliderInt("Lights", &state.light_count, 0, 256, "")) state.isreference = false;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextUnformatted(state.istimed ? "TIMED RESULT / MEAN" : "LIVE / 0.5 s MEAN");
    ImGui::TextWrapped("B: %s", state.current_label.c_str());
    if (state.isreference) ImGui::TextWrapped("A: %s", state.reference_label.c_str());
    if (ImGui::BeginTable("Metrics", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 148);
        ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Saved %", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        auto row = [&](const char* label, double a, double b, bool iscount = false)
        {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            if (state.isreference && std::isfinite(a)) ImGui::Text(iscount ? "%.0f" : "%.3f", a); else ImGui::TextUnformatted("--");
            ImGui::TableNextColumn(); ImGui::Text(iscount ? "%.0f" : "%.3f", b);
            ImGui::TableNextColumn();
            if (state.isreference && std::isfinite(a) && a > 0)
            {
                const double change = (a - b) / a * 100;
                ImGui::TextColored(change >= 0 ? ImVec4(.45f,.85f,.65f,1) : ImVec4(1,.55f,.45f,1), "%+.1f", change);
            }
            else ImGui::TextUnformatted("--");
        };
        const auto& a = state.reference; const auto& b = state.current;
        row("CPU prepare ms", a.prepare, b.prepare);
        row("CPU record ms", a.record, b.record);
        row("GPU total ms", a.gpu, b.gpu);
        const bool issame_renderer = state.reference_renderer == state.renderer;
        row(isforward ? "Mesh + light ms" : "G-buffer ms", issame_renderer ? a.mesh : NAN, b.mesh);
        if (!isforward) row("Lighting ms", issame_renderer ? a.light : NAN, b.light);
        row("Mesh draws", static_cast<double>(a.draws), static_cast<double>(b.draws), true);
        row("SRV writes / frame", static_cast<double>(a.srvs), static_cast<double>(b.srvs), true);
        row("G-buffer payload MiB", a.gbuffer_mib, b.gbuffer_mib);
        ImGui::EndTable();
    }
    ImGui::Text("B samples: %llu", static_cast<unsigned long long>(state.current.frames));
    if (state.isreference) { ImGui::SameLine(); ImGui::Text("A: %llu", static_cast<unsigned long long>(state.reference.frames)); }
    ImGui::BeginDisabled(state.islocked || state.current.frames == 0);
    if (ImGui::Button("Store live A"))
    {
        state.reference = state.current; state.isreference = true;
        state.reference_label = std::string(state.renderer) + " / live snapshot";
        state.reference_renderer = state.renderer;
    }
    ImGui::EndDisabled();
    ImGui::End();
}
