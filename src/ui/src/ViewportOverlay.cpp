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
    const float sx = (ndc.x * 0.5F + 0.5F) * fbSize.x;
    const float sy = (ndc.y * 0.5F + 0.5F) * fbSize.y;
    return ImVec2{sx, sy};
}

void DrawMeasurePoint(ImDrawList* dl, const ImVec2& p, ImU32 color) {
    dl->AddCircleFilled(p, 5.0F, color);
    dl->AddCircle(p, 5.0F, IM_COL32(0, 0, 0, 255), 0, 1.5F);
}

void DrawAxesGizmo(glm::vec3 forward) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float size = 40.0F;
    const ImVec2 center{origin.x + size, origin.y + size};

    struct Axis {
        glm::vec3 dir;
        ImU32 color;
        const char* label;
    };
    const Axis axes[] = {
        {{1.0F, 0.0F, 0.0F}, IM_COL32(220, 50, 50, 255), "X"},
        {{0.0F, 1.0F, 0.0F}, IM_COL32(50, 200, 50, 255), "Y"},
        {{0.0F, 0.0F, 1.0F}, IM_COL32(80, 120, 240, 255), "Z"},
    };

    const glm::vec3 f = forward;
    const glm::vec3 up{0.0F, 1.0F, 0.0F};
    const glm::vec3 right{f.z, 0.0F, -f.x};

    for (const Axis& axis : axes) {
        const float px = axis.dir.x * right.x + axis.dir.y * right.y + axis.dir.z * right.z;
        const float py = axis.dir.x * up.x + axis.dir.y * up.y + axis.dir.z * up.z;
        const ImVec2 tip{center.x + px * size, center.y - py * size};
        drawList->AddLine(center, tip, axis.color, 2.0F);
        drawList->AddText(tip, axis.color, axis.label);
    }

    ImGui::Dummy(ImVec2(size * 2.0F, size * 2.0F));
}

}  // namespace

const char* ViewportOverlay::GetName() const {
    return "Viewport Overlay";
}

void ViewportOverlay::SetFps(float fps) {
    m_fps = fps;
}

float ViewportOverlay::GetFps() const {
    return m_fps;
}

void ViewportOverlay::SetCameraPosition(glm::vec3 position) {
    m_cameraPosition = position;
}

glm::vec3 ViewportOverlay::GetCameraPosition() const {
    return m_cameraPosition;
}

void ViewportOverlay::SetCameraForward(glm::vec3 forward) {
    m_cameraForward = forward;
}

glm::vec3 ViewportOverlay::GetCameraForward() const {
    return m_cameraForward;
}

void ViewportOverlay::SetFpsCounterVisible(bool visible) {
    m_fpsCounterVisible = visible;
}

bool ViewportOverlay::IsFpsCounterVisible() const {
    return m_fpsCounterVisible;
}

void ViewportOverlay::SetCameraInfoVisible(bool visible) {
    m_cameraInfoVisible = visible;
}

bool ViewportOverlay::IsCameraInfoVisible() const {
    return m_cameraInfoVisible;
}

void ViewportOverlay::SetAxesGizmoVisible(bool visible) {
    m_axesGizmoVisible = visible;
}

bool ViewportOverlay::IsAxesGizmoVisible() const {
    return m_axesGizmoVisible;
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
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::SetNextWindowBgAlpha(0.35F);
    if (ImGui::Begin(GetName(), nullptr, flags)) {
        if (m_fpsCounterVisible) {
            ImGui::Text("FPS: %.1f", static_cast<double>(m_fps));
        }
        if (m_cameraInfoVisible) {
            ImGui::Text("Pos: %.2f, %.2f, %.2f", static_cast<double>(m_cameraPosition.x),
                        static_cast<double>(m_cameraPosition.y), static_cast<double>(m_cameraPosition.z));
            ImGui::Text("Fwd: %.2f, %.2f, %.2f", static_cast<double>(m_cameraForward.x),
                        static_cast<double>(m_cameraForward.y), static_cast<double>(m_cameraForward.z));
        }
        if (m_axesGizmoVisible) {
            ImGui::Separator();
            DrawAxesGizmo(m_cameraForward);
        }
    }
    ImGui::End();

    if (m_measureTool != nullptr && m_measureFbSize.x > 0.0F && m_measureFbSize.y > 0.0F) {
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
}

}  // namespace bimeup::ui
