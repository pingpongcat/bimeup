#include <ui/ViewportOverlay.h>

#include <imgui.h>

namespace bimeup::ui {

namespace {

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
}

}  // namespace bimeup::ui
