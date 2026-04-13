#include <ui/MeasurementsPanel.h>

#include <imgui.h>
#include <scene/Measurement.h>

#include <algorithm>
#include <cstdio>
#include <optional>

namespace bimeup::ui {

const char* MeasurementsPanel::GetName() const {
    return "Measurements";
}

void MeasurementsPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    if (m_tool == nullptr) {
        ImGui::TextDisabled("Measure tool inactive.");
        ImGui::End();
        return;
    }

    const auto& items = m_tool->GetMeasurements();
    ImGui::Text("Saved: %zu", items.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear all") && m_onClearAll) {
        m_onClearAll();
    }
    ImGui::SameLine();
    const bool anyVisible = std::any_of(items.begin(), items.end(),
                                        [](const auto& r) { return r.visible; });
    if (ImGui::SmallButton(anyVisible ? "Hide all" : "Show all")) {
        m_tool->SetAllVisibility(!anyVisible);
    }
    ImGui::Separator();

    if (items.empty()) {
        ImGui::TextDisabled("Click two points in the viewport to record a measurement.");
        ImGui::End();
        return;
    }

    std::optional<std::size_t> toRemove;

    if (ImGui::BeginTable("measurements", 6,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Distance");
        ImGui::TableSetupColumn("From");
        ImGui::TableSetupColumn("To");
        ImGui::TableSetupColumn("Show");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& r = items[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%zu", i + 1);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f m", static_cast<double>(r.distance));
            ImGui::TableNextColumn();
            ImGui::Text("(%.2f, %.2f, %.2f)", static_cast<double>(r.pointA.x),
                        static_cast<double>(r.pointA.y), static_cast<double>(r.pointA.z));
            ImGui::TableNextColumn();
            ImGui::Text("(%.2f, %.2f, %.2f)", static_cast<double>(r.pointB.x),
                        static_cast<double>(r.pointB.y), static_cast<double>(r.pointB.z));
            ImGui::TableNextColumn();
            bool visible = r.visible;
            if (ImGui::Checkbox("##vis", &visible)) {
                m_tool->SetVisibility(i, visible);
            }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X")) {
                toRemove = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (toRemove.has_value()) {
        m_tool->RemoveMeasurement(*toRemove);
    }

    ImGui::End();
}

}  // namespace bimeup::ui
