#include <ui/RenderQualityPanel.h>

#include <imgui.h>

#include <glm/geometric.hpp>

#include <array>
#include <cstdio>

namespace bimeup::ui {

namespace {

void DrawSkyColor(const char* label, glm::vec3& c) {
    std::array<float, 3> rgb{c.r, c.g, c.b};
    if (ImGui::ColorEdit3(label, rgb.data())) {
        c = glm::vec3(rgb[0], rgb[1], rgb[2]);
    }
}

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

        ImGui::TextUnformatted("Sky ambient (hemisphere)");
        DrawSkyColor("Zenith", m_settings.lighting.sky.zenith);
        DrawSkyColor("Horizon", m_settings.lighting.sky.horizon);
        DrawSkyColor("Ground", m_settings.lighting.sky.ground);

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

    if (ImGui::CollapsingHeader("Selection outline")) {
        auto& outline = m_settings.outline;
        ImGui::Checkbox("Enabled", &outline.enabled);

        ImGui::BeginDisabled(!outline.enabled);
        std::array<float, 4> sel{outline.selectedColor.r, outline.selectedColor.g,
                                 outline.selectedColor.b, outline.selectedColor.a};
        if (ImGui::ColorEdit4("Selected", sel.data())) {
            outline.selectedColor = glm::vec4(sel[0], sel[1], sel[2], sel[3]);
        }
        std::array<float, 4> hov{outline.hoverColor.r, outline.hoverColor.g,
                                 outline.hoverColor.b, outline.hoverColor.a};
        if (ImGui::ColorEdit4("Hover", hov.data())) {
            outline.hoverColor = glm::vec4(hov[0], hov[1], hov[2], hov[3]);
        }
        ImGui::SliderFloat("Thickness (px)", &outline.thickness, 1.0F, 6.0F, "%.1f");
        ImGui::SliderFloat("Depth edge (m)", &outline.depthEdgeThreshold, 0.001F, 1.0F,
                           "%.3f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("SSIL")) {
        auto& ssil = m_settings.ssil;
        ImGui::Checkbox("Enabled", &ssil.enabled);
        ImGui::BeginDisabled(!ssil.enabled);
        ImGui::SliderFloat("Radius (m)", &ssil.radius, 0.05F, 2.0F, "%.2f");
        ImGui::SliderFloat("Intensity", &ssil.intensity, 0.0F, 4.0F, "%.2f");
        ImGui::SliderFloat("Normal rejection", &ssil.normalRejection, 0.0F, 16.0F, "%.1f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("FXAA")) {
        auto& fxaa = m_settings.fxaa;
        ImGui::Checkbox("Enabled", &fxaa.enabled);
        ImGui::BeginDisabled(!fxaa.enabled);
        ImGui::TextUnformatted("Quality");
        ImGui::SameLine();
        if (ImGui::RadioButton("LOW", fxaa.quality == 0)) {
            fxaa.quality = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("HIGH", fxaa.quality == 1)) {
            fxaa.quality = 1;
        }
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Fog")) {
        auto& fog = m_settings.fog;
        ImGui::Checkbox("Enabled", &fog.enabled);
        ImGui::BeginDisabled(!fog.enabled);
        std::array<float, 3> col{fog.color.r, fog.color.g, fog.color.b};
        if (ImGui::ColorEdit3("Color", col.data())) {
            fog.color = glm::vec3(col[0], col[1], col[2]);
        }
        ImGui::SliderFloat("Start (m)", &fog.start, 0.0F, 500.0F, "%.1f");
        ImGui::SliderFloat("End (m)", &fog.end, 0.0F, 1000.0F, "%.1f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Bloom")) {
        auto& bloom = m_settings.bloom;
        ImGui::Checkbox("Enabled", &bloom.enabled);
        ImGui::BeginDisabled(!bloom.enabled);
        ImGui::SliderFloat("Threshold", &bloom.threshold, 0.0F, 5.0F, "%.2f");
        ImGui::SliderFloat("Intensity", &bloom.intensity, 0.0F, 0.5F, "%.3f");
        ImGui::EndDisabled();
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

    ImGui::End();
}

}  // namespace bimeup::ui
