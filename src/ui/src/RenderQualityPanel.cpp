#include <ui/RenderQualityPanel.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace bimeup::ui {

namespace {

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kDegToRad = kPi / 180.0;

// Gregorian calendar → Julian Day at midnight UTC (Fliegel/Van Flandern).
// Adding `hourUtc / 24` shifts to the requested instant.
double CalendarToJulianDayUtc(int year, int month, int day, float hourUtc) {
    const int a = (14 - month) / 12;
    const int y = year + 4800 - a;
    const int m = month + 12 * a - 3;
    const int jdn = day + ((153 * m) + 2) / 5 + (365 * y) + (y / 4) - (y / 100) +
                    (y / 400) - 32045;
    return static_cast<double>(jdn) - 0.5 + static_cast<double>(hourUtc) / 24.0;
}

// Crude solar-time offset: 15° of longitude ≈ 1 h. Good enough for a
// lighting-preview slider — users can nudge the hour field for finer tz.
float HourLocalToUtc(float hourLocal, double longitudeRad) {
    const double offsetHours = longitudeRad * (12.0 / kPi);
    return hourLocal - static_cast<float>(offsetHours);
}

int MaxDayForMonth(int month, int year) {
    constexpr std::array<int, 12> kDays{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = kDays[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (leap) {
            d = 29;
        }
    }
    return d;
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

    // Stage 9.8.d — render-mode switch. HybridRt greyed out when the
    // device probe says no; tooltip explains why. `PathTraced` is a
    // 9.9 hook, always greyed out for now.
    if (ImGui::CollapsingHeader("Mode")) {
        auto& mode = m_settings.mode;
        if (ImGui::RadioButton("Rasterised", mode == RenderMode::Rasterised)) {
            mode = RenderMode::Rasterised;
        }
        ImGui::BeginDisabled(!m_settings.rayTracingAvailable);
        if (ImGui::RadioButton("Hybrid RT", mode == RenderMode::HybridRt)) {
            mode = RenderMode::HybridRt;
        }
        ImGui::EndDisabled();
        if (!m_settings.rayTracingAvailable && ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("GPU lacks VK_KHR_ray_tracing_pipeline");
        }
        ImGui::BeginDisabled(true);
        bool pt = mode == RenderMode::PathTraced;
        ImGui::RadioButton("Path Traced", pt);
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Sun")) {
        auto& s = m_settings;

        ImGui::SliderInt("Month", &s.month, 1, 12);
        const int maxDay = MaxDayForMonth(s.month, s.year);
        s.day = std::clamp(s.day, 1, maxDay);
        ImGui::SliderInt("Day", &s.day, 1, maxDay);
        ImGui::SliderFloat("Hour (local)", &s.hourLocal, 0.0F, 24.0F, "%.2f");

        ImGui::Checkbox("Use site geolocation", &s.useSiteGeolocation);

        ImGui::BeginDisabled(s.useSiteGeolocation);
        float latDeg = static_cast<float>(s.sun.siteLocation.latitudeRad * kRadToDeg);
        float lonDeg = static_cast<float>(s.sun.siteLocation.longitudeRad * kRadToDeg);
        if (ImGui::SliderFloat("Latitude (°)", &latDeg, -90.0F, 90.0F, "%.3f")) {
            s.sun.siteLocation.latitudeRad = static_cast<double>(latDeg) * kDegToRad;
        }
        if (ImGui::SliderFloat("Longitude (°)", &lonDeg, -180.0F, 180.0F, "%.3f")) {
            s.sun.siteLocation.longitudeRad = static_cast<double>(lonDeg) * kDegToRad;
        }
        ImGui::EndDisabled();

        ImGui::Checkbox("Artificial interior lights", &s.sun.indoorLightsEnabled);

        const float hourUtc = HourLocalToUtc(s.hourLocal, s.sun.siteLocation.longitudeRad);
        s.sun.julianDayUtc = CalendarToJulianDayUtc(s.year, s.month, s.day, hourUtc);
    }

    if (ImGui::CollapsingHeader("Tonemap")) {
        ImGui::SliderFloat("Exposure", &m_settings.sun.exposure, 0.1F, 2.0F, "%.2f");
    }

    if (ImGui::CollapsingHeader("Ambient occlusion")) {
        auto& ssao = m_settings.ssao;
        ImGui::SliderFloat("Radius (m)", &ssao.radius, 0.05F, 2.0F, "%.2f");
        ImGui::SliderFloat("Falloff", &ssao.falloff, 0.0F, 1.0F, "%.2f");
        ImGui::SliderFloat("Intensity", &ssao.intensity, 0.0F, 2.0F, "%.2f");
        ImGui::SliderFloat("Shadow power", &ssao.shadowPower, 0.5F, 3.0F, "%.2f");
    }

    if (ImGui::CollapsingHeader("Edges")) {
        auto& edges = m_settings.edges;
        ImGui::Checkbox("Enabled", &edges.enabled);
        ImGui::BeginDisabled(!edges.enabled);
        ImGui::ColorEdit3("Color", &edges.color.x);
        ImGui::SliderFloat("Opacity", &edges.opacity, 0.0F, 1.0F, "%.2f");
        // Width > 1.0 requires the device's `wideLines` feature; main.cpp
        // clamps the pushed value to 1.0 when unavailable. Slider range
        // stays [1, 4] so users see the intended max even on older GPUs.
        ImGui::SliderFloat("Width (px)", &edges.width, 1.0F, 4.0F, "%.1f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("SMAA")) {
        auto& smaa = m_settings.smaa;
        ImGui::Checkbox("Enabled", &smaa.enabled);
        ImGui::BeginDisabled(!smaa.enabled);
        ImGui::SliderFloat("Threshold", &smaa.threshold, 0.05F, 0.2F, "%.3f");
        // RP.19 — quality selects iryoku's LOW/MEDIUM/HIGH max-search-steps
        // preset. HIGH (16/8) is the pre-RP.19 default; LOW/MEDIUM trade edge
        // travel for fill rate on long thin aliased runs.
        static constexpr std::array<const char*, 3> kLabels{"Low", "Medium", "High"};
        static constexpr std::array<SmaaQuality, 3> kValues{
            SmaaQuality::Low, SmaaQuality::Medium, SmaaQuality::High};
        ImGui::TextUnformatted("Quality");
        ImGui::SameLine();
        for (std::size_t i = 0; i < kValues.size(); ++i) {
            if (ImGui::RadioButton(kLabels[i], smaa.quality == kValues[i])) {
                smaa.quality = kValues[i];
            }
            if (i + 1 < kValues.size()) {
                ImGui::SameLine();
            }
        }
        ImGui::EndDisabled();
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
        ImGui::Checkbox("Window transmission", &shadow.windowTransmission);
        ImGui::EndDisabled();
    }

    ImGui::End();
}

}  // namespace bimeup::ui
