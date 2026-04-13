#include <ui/MeasurementsPanel.h>

#include <imgui.h>
#include <scene/Measurement.h>

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
    ImGui::Separator();

    if (items.empty()) {
        ImGui::TextDisabled("Click two points in the viewport to record a measurement.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("measurements", 4,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Distance");
        ImGui::TableSetupColumn("From");
        ImGui::TableSetupColumn("To");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < items.size(); ++i) {
            const auto& r = items[i];
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
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

}  // namespace bimeup::ui
