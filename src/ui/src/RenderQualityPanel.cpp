#include <ui/RenderQualityPanel.h>

#include <imgui.h>

#include <glm/geometric.hpp>

#include <array>
#include <cstdio>

namespace bimeup::ui {

namespace {

void DrawDirectionalLight(const char* label, renderer::DirectionalLight& light) {
    ImGui::PushID(label);
    ImGui::Checkbox(label, &light.enabled);

    ImGui::BeginDisabled(!light.enabled);
    std::array<float, 3> dir{light.direction.x, light.direction.y, light.direction.z};
    if (ImGui::SliderFloat3("Direction", dir.data(), -1.0F, 1.0F)) {
        glm::vec3 v(dir[0], dir[1], dir[2]);
        float len = glm::length(v);
        light.direction = (len > 1e-4F) ? (v / len) : glm::vec3(0.0F, -1.0F, 0.0F);
    }
    std::array<float, 3> color{light.color.r, light.color.g, light.color.b};
    if (ImGui::ColorEdit3("Color", color.data())) {
        light.color = glm::vec3(color[0], color[1], color[2]);
    }
    ImGui::SliderFloat("Intensity", &light.intensity, 0.0F, 3.0F);
    ImGui::EndDisabled();
    ImGui::PopID();
}

}  // namespace

const char* RenderQualityPanel::GetName() const {
    return "Render Quality";
}

void RenderQualityPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Three-point lighting")) {
        DrawDirectionalLight("Key", m_settings.lighting.key);
        ImGui::Separator();
        DrawDirectionalLight("Fill", m_settings.lighting.fill);
        ImGui::Separator();
        DrawDirectionalLight("Rim", m_settings.lighting.rim);
        ImGui::Separator();

        std::array<float, 3> amb{m_settings.lighting.ambient.r, m_settings.lighting.ambient.g,
                                 m_settings.lighting.ambient.b};
        if (ImGui::ColorEdit3("Ambient", amb.data())) {
            m_settings.lighting.ambient = glm::vec3(amb[0], amb[1], amb[2]);
        }

        if (ImGui::Button("Reset to defaults")) {
            m_settings.lighting = renderer::MakeDefaultLighting();
        }
    }

    if (ImGui::CollapsingHeader("MSAA")) {
        static constexpr std::array<int, 4> kSamples{1, 2, 4, 8};
        for (int s : kSamples) {
            char label[8];
            std::snprintf(label, sizeof(label), "%dx", s);
            if (ImGui::RadioButton(label, m_settings.msaaSamples == s)) {
                m_settings.msaaSamples = s;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    if (ImGui::CollapsingHeader("Shadows")) {
        auto& shadow = m_settings.lighting.shadow;
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

    if (ImGui::CollapsingHeader("SSAO")) {
        ImGui::TextDisabled("(Stage R.4 — not yet wired)");
        ImGui::Checkbox("Enabled", &m_settings.ssaoEnabled);
        ImGui::SliderFloat("Radius", &m_settings.ssaoRadius, 0.05F, 2.0F);
        ImGui::SliderFloat("Bias", &m_settings.ssaoBias, 0.0F, 0.1F);
    }

    ImGui::End();
}

}  // namespace bimeup::ui
