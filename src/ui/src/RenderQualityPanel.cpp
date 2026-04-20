#include <ui/RenderQualityPanel.h>

#include <imgui.h>

#include <array>
#include <cstdio>

namespace bimeup::ui {

const char* RenderQualityPanel::GetName() const {
    return "Render Quality";
}

void RenderQualityPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    // RP.16.4.b — three-point + sky-colour sections retired. The Sun widgets
    // (date / time / site / indoor-preset) land in RP.16.6.

    if (ImGui::CollapsingHeader("Tonemap")) {
        ImGui::SliderFloat("Exposure", &m_settings.sun.exposure, 0.1F, 2.0F, "%.2f");
    }

    if (ImGui::CollapsingHeader("SMAA")) {
        auto& smaa = m_settings.smaa;
        ImGui::Checkbox("Enabled", &smaa.enabled);
    }

    if (ImGui::CollapsingHeader("Shadows")) {
        auto& shadow = m_settings.sun.shadow;
        ImGui::Checkbox("Enabled", &shadow.enabled);

        ImGui::BeginDisabled(!shadow.enabled);
        static constexpr std::array<std::uint32_t, 4> kResolutions{512U, 1024U, 2048U, 4096U};
        ImGui::TextUnformatted("Resolution");
        ImGui::SameLine();
        for (std::uint32_t r : kResolutions) {
            char label[8];
            std::snprintf(label, sizeof(label), "%u", r);
            if (ImGui::RadioButton(label, shadow.mapResolution == r)) {
                shadow.mapResolution = r;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();

        ImGui::SliderFloat("Bias", &shadow.bias, 0.0F, 0.02F, "%.4f");
        ImGui::SliderFloat("PCF radius (texels)", &shadow.pcfRadius, 0.0F, 4.0F);
        ImGui::EndDisabled();
    }

    ImGui::End();
}

}  // namespace bimeup::ui
