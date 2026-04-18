#include <ui/ViewportOverlay.h>

#include <imgui.h>
#include <scene/Measurement.h>

#include <cstdio>
#include <optional>

namespace bimeup::ui {

namespace {

std::optional<ImVec2> WorldToScreen(const glm::vec3& worldPos,
                                    const glm::mat4& view,
                                    const glm::mat4& proj,
                                    const glm::vec2& fbSize) {
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0F);
    if (clip.w <= 0.0F) {
        return std::nullopt;  // behind camera
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.0F || ndc.x > 1.0F || ndc.y < -1.0F || ndc.y > 1.0F) {
        return std::nullopt;
    }
    // Overlay receives the view/proj used by picking (Vulkan Y-flip undone),
    // so NDC is OpenGL-convention (y-up). ImGui screen coords are y-down.
    const float sx = (ndc.x * 0.5F + 0.5F) * fbSize.x;
    const float sy = (1.0F - (ndc.y * 0.5F + 0.5F)) * fbSize.y;
    return ImVec2{sx, sy};
}

void DrawMeasurePoint(ImDrawList* dl, const ImVec2& p, ImU32 color) {
    dl->AddCircleFilled(p, 5.0F, color);
    dl->AddCircle(p, 5.0F, IM_COL32(0, 0, 0, 255), 0, 1.5F);
}

}  // namespace

const char* ViewportOverlay::GetName() const {
    return "Viewport Overlay";
}

void ViewportOverlay::SetMeasurement(const scene::MeasureTool* tool,
                                     const glm::mat4& view,
                                     const glm::mat4& proj,
                                     glm::vec2 framebufferSize) {
    m_measureTool = tool;
    m_measureView = view;
    m_measureProj = proj;
    m_measureFbSize = framebufferSize;
}

void ViewportOverlay::SetSnapCandidate(std::optional<glm::vec3> world, bool isVertex) {
    m_snapCandidate = world;
    m_snapIsVertex = isVertex;
}

void ViewportOverlay::OnDraw() {
    if (m_measureTool == nullptr || m_measureFbSize.x <= 0.0F || m_measureFbSize.y <= 0.0F) {
        return;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 colSaved = IM_COL32(255, 220, 60, 255);
    const ImU32 lineSaved = IM_COL32(255, 220, 60, 200);
    const ImU32 colLive = IM_COL32(120, 220, 255, 255);
    const ImU32 lineLive = IM_COL32(120, 220, 255, 220);
    const ImU32 vertexSnap = IM_COL32(80, 255, 160, 255);
    const ImU32 faceSnap = IM_COL32(220, 220, 220, 200);

    auto drawLabel = [&](const ImVec2& at, const char* text) {
        const float w = ImGui::CalcTextSize(text).x + 8.0F;
        dl->AddRectFilled({at.x - 4.0F, at.y - 2.0F},
                          {at.x + w, at.y + 16.0F}, IM_COL32(0, 0, 0, 170), 3.0F);
        dl->AddText(at, IM_COL32(255, 255, 255, 255), text);
    };

    // Saved measurements.
    for (const auto& r : m_measureTool->GetMeasurements()) {
        if (!r.visible) {
            continue;
        }
        auto a = WorldToScreen(r.pointA, m_measureView, m_measureProj, m_measureFbSize);
        auto b = WorldToScreen(r.pointB, m_measureView, m_measureProj, m_measureFbSize);
        if (a && b) {
            dl->AddLine(*a, *b, lineSaved, 2.0F);
            DrawMeasurePoint(dl, *a, colSaved);
            DrawMeasurePoint(dl, *b, colSaved);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.3f m", static_cast<double>(r.distance));
            drawLabel({(a->x + b->x) * 0.5F, (a->y + b->y) * 0.5F - 14.0F}, buf);
        }
    }

    // In-progress: first point + live preview to snap candidate.
    const auto& first = m_measureTool->GetFirstPoint();
    if (first.has_value()) {
        auto a = WorldToScreen(*first, m_measureView, m_measureProj, m_measureFbSize);
        if (a) {
            DrawMeasurePoint(dl, *a, colLive);
            if (m_snapCandidate.has_value()) {
                auto b = WorldToScreen(*m_snapCandidate, m_measureView, m_measureProj,
                                       m_measureFbSize);
                if (b) {
                    dl->AddLine(*a, *b, lineLive, 2.0F);
                    const float d = glm::length(*m_snapCandidate - *first);
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%.3f m", static_cast<double>(d));
                    drawLabel({(a->x + b->x) * 0.5F, (a->y + b->y) * 0.5F - 14.0F}, buf);
                }
            }
        }
    }

    // Snap candidate marker (cursor-following hint).
    if (m_snapCandidate.has_value()) {
        auto s = WorldToScreen(*m_snapCandidate, m_measureView, m_measureProj,
                               m_measureFbSize);
        if (s) {
            if (m_snapIsVertex) {
                // Diamond around snapped vertex.
                const float r = 7.0F;
                dl->AddQuad({s->x - r, s->y}, {s->x, s->y - r}, {s->x + r, s->y},
                            {s->x, s->y + r}, vertexSnap, 2.0F);
                dl->AddCircleFilled(*s, 2.5F, vertexSnap);
            } else {
                dl->AddCircle(*s, 4.0F, faceSnap, 0, 1.5F);
            }
        }
    }
}

}  // namespace bimeup::ui
