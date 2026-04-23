// Single-header custom axis-section gizmo — replaces ImGuizmo for axis-locked
// translate + mode-switch + close buttons on a clip plane.
//
// Style matches imoguizmo: internal namespace for math, stateful static config
// for the drag handle, one public entry point per frame. The math helpers
// (ProjectWorldToScreen, AxisDragDelta) don't touch ImGui so they can be
// unit-tested without a live context.

#pragma once

#include <glm/glm.hpp>

#include <optional>

#ifndef BIMEUP_AXIS_SECTION_GIZMO_MATH_ONLY
#include <imgui.h>
#include <scene/AxisSectionController.h>

#include <cstdio>
#endif

namespace bimeup::ui {

// --- Pure math ---------------------------------------------------------------

// Project a world-space point through view·projection into ImGui screen space
// (Y-down, origin top-left). The projection matrix is GL-convention (Y up);
// callers with a Vulkan projection must flip proj[1][1] before passing it in.
// Returns nullopt if the point is at or behind the near plane (clip.w <= 1e-4).
inline std::optional<glm::vec2> ProjectWorldToScreen(
    const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& worldPoint, const glm::vec2& displaySize) {
    const glm::vec4 clip = projection * view * glm::vec4(worldPoint, 1.0F);
    if (clip.w <= 1e-4F) return std::nullopt;
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    return glm::vec2{(ndcX * 0.5F + 0.5F) * displaySize.x,
                     (0.5F - ndcY * 0.5F) * displaySize.y};
}

// Sign of the drag-handle bar direction along the axis. The bar extends
// from the plane origin toward the material being cut away (opposite of the
// kept side), so for CutFront / SectionOnly the bar points -axis and for
// CutBack it points +axis. Parameterised on a bool so this stays math-only
// (no enum dependency).
inline float HandleBarSign(bool isCutBack) {
    return isCutBack ? 1.0F : -1.0F;
}

// Invert a mouse-drag delta onto an axis that has been projected into screen
// space. `screenAxisStart` / `screenAxisEnd` are the screen-space projections
// of two world-space points 1 unit apart along the axis (so their delta is
// pixels-per-world-unit). `mouseDelta` is the cursor displacement in pixels.
// Returns the world-space offset delta along the axis.
// Returns nullopt if the screen-projected axis is shorter than `minPixels`
// (axis nearly parallel to camera ray → drag would be unstable).
inline std::optional<float> AxisDragDelta(const glm::vec2& screenAxisStart,
                                          const glm::vec2& screenAxisEnd,
                                          const glm::vec2& mouseDelta,
                                          float minPixels) {
    const glm::vec2 axisScreen = screenAxisEnd - screenAxisStart;
    const float lenSq = glm::dot(axisScreen, axisScreen);
    if (lenSq < minPixels * minPixels) return std::nullopt;
    return glm::dot(mouseDelta, axisScreen) / lenSq;
}

#ifndef BIMEUP_AXIS_SECTION_GIZMO_MATH_ONLY

// --- ImGui gizmo -------------------------------------------------------------

namespace internal {

struct DragState {
    bool active = false;
    scene::Axis axis = scene::Axis::X;
    float startOffset = 0.0F;
    glm::vec2 startMousePos{0.0F};
    glm::vec2 startAxisP0{0.0F};
    glm::vec2 startAxisP1{0.0F};
};

inline DragState& GetDragState() {
    static DragState s;
    return s;
}

struct RegionResult {
    bool hovered = false;
    bool active = false;
    bool pressed = false;
};

// Place an invisible hit-test region at screen-space `pos` with `size`.
// Uses a tiny transparent host window + InvisibleButton so ImGui's mouse
// capture behaves correctly (dragging here won't start viewport orbit).
inline RegionResult HitRegion(const char* id, ImVec2 pos, ImVec2 size) {
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin(id, nullptr, kFlags);
    ImGui::InvisibleButton("##hit", size);
    RegionResult r{ImGui::IsItemHovered(), ImGui::IsItemActive(),
                   ImGui::IsItemClicked()};
    ImGui::End();
    ImGui::PopStyleVar();
    return r;
}

inline glm::vec3 AxisUnit(scene::Axis axis) {
    switch (axis) {
        case scene::Axis::X: return {1.0F, 0.0F, 0.0F};
        case scene::Axis::Y: return {0.0F, 1.0F, 0.0F};
        case scene::Axis::Z: return {0.0F, 0.0F, 1.0F};
    }
    return {1.0F, 0.0F, 0.0F};
}

inline const char* AxisLetter(scene::Axis axis) {
    switch (axis) {
        case scene::Axis::X: return "X";
        case scene::Axis::Y: return "Y";
        case scene::Axis::Z: return "Z";
    }
    return "?";
}

}  // namespace internal

// Draw the axis-section gizmo for one slot.
// Elements laid out along the screen-projected axis starting at the plane
// origin: drag line → grab circle → [B][F][C] mode pills → (×) close button.
// Returns true if any of (`offset`, `mode`, `open`) changed this frame.
inline bool DrawAxisHandle(scene::Axis axis, float& offset,
                           scene::SectionMode& mode, bool& open,
                           const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec2& displaySize) {
    const glm::vec3 axisUnit = internal::AxisUnit(axis);
    const glm::vec3 origin3 = axisUnit * offset;
    // Bar points toward the cut-away side (opposite of the kept half).
    const float barSign =
        HandleBarSign(mode == scene::SectionMode::CutBack);

    const auto originOpt =
        ProjectWorldToScreen(view, projection, origin3, displaySize);
    const auto barTipOpt = ProjectWorldToScreen(
        view, projection, origin3 + axisUnit * barSign, displaySize);
    const auto unitTipOpt = ProjectWorldToScreen(
        view, projection, origin3 + axisUnit, displaySize);
    if (!originOpt.has_value() || !barTipOpt.has_value() ||
        !unitTipOpt.has_value()) {
        return false;
    }
    const glm::vec2 originPx = *originOpt;
    const glm::vec2 barTipPx = *barTipOpt;
    const glm::vec2 unitTipPx = *unitTipOpt;

    const glm::vec2 rawDir = barTipPx - originPx;
    const float dirLen = glm::length(rawDir);
    if (dirLen < 8.0F) return false;  // axis nearly edge-on → skip
    const glm::vec2 dir = rawDir / dirLen;

    // Drag bar + grab follow the projected axis direction. The button row is
    // anchored to the grab but laid out along screen-horizontal so the pills
    // stay in a readable row regardless of camera orientation.
    constexpr float kGrabOffset = 54.0F;
    constexpr float kPillWidth = 22.0F;
    constexpr float kGrabToGroupGap = 16.0F;
    constexpr float kCloseGap = 8.0F;
    constexpr float kCloseRaise = 18.0F;
    constexpr glm::vec2 kHorizontal{1.0F, 0.0F};

    const glm::vec2 grabPx = originPx + dir * kGrabOffset;
    const glm::vec2 groupStart = grabPx + kHorizontal * kGrabToGroupGap;
    const glm::vec2 fPx = groupStart + kHorizontal * (kPillWidth * 0.5F);
    const glm::vec2 sPx = groupStart + kHorizontal * (kPillWidth * 1.5F);
    const glm::vec2 bPx = groupStart + kHorizontal * (kPillWidth * 2.5F);
    const glm::vec2 xPx{bPx.x + kPillWidth * 0.5F + kCloseGap + 9.0F,
                        bPx.y - kCloseRaise};

    const char* axisLetter = internal::AxisLetter(axis);
    char idBuf[64];
    bool changed = false;

    // --- Grab handle (drag to change offset) ---
    std::snprintf(idBuf, sizeof(idBuf), "##asg_grab_%s", axisLetter);
    const internal::RegionResult grab = internal::HitRegion(
        idBuf, ImVec2{grabPx.x - 11.0F, grabPx.y - 11.0F}, ImVec2{22.0F, 22.0F});

    auto& drag = internal::GetDragState();
    if (grab.active) {
        if (!drag.active || drag.axis != axis) {
            drag.active = true;
            drag.axis = axis;
            drag.startOffset = offset;
            drag.startMousePos = {ImGui::GetIO().MousePos.x,
                                  ImGui::GetIO().MousePos.y};
            drag.startAxisP0 = originPx;
            drag.startAxisP1 = unitTipPx;
        }
        const glm::vec2 mp{ImGui::GetIO().MousePos.x,
                           ImGui::GetIO().MousePos.y};
        const glm::vec2 mouseDelta = mp - drag.startMousePos;
        if (const auto d = AxisDragDelta(drag.startAxisP0, drag.startAxisP1,
                                         mouseDelta, 8.0F)) {
            const float newOffset = drag.startOffset + *d;
            if (newOffset != offset) {
                offset = newOffset;
                changed = true;
            }
        }
    } else if (drag.active && drag.axis == axis) {
        drag.active = false;
    }

    // --- Drag bar + origin dot + grab circle (drawn first so F|S|B pills
    //     and the × close button sit on top — the grab is the bottom of the
    //     z-stack within the handle). Background draw list so the whole
    //     handle sits behind any panels the user opens, matching the nav
    //     gizmo's stacking.
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        // Per-axis color (CG convention). UI labels "Y" and "Z" are swapped
        // relative to the internal `Axis` enum (BIM convention, see
        // AxisSectionPanel.cpp kAxisButtons), so world Axis::Y is the UI "Z"
        // (blue / vertical) and world Axis::Z is the UI "Y" (green / depth).
        // SectionOnly overrides to amber across all axes as a mode cue.
        const ImU32 perAxisColor = [axis]() -> ImU32 {
            switch (axis) {
                case scene::Axis::X: return IM_COL32(230, 70, 70, 255);   // red
                case scene::Axis::Y: return IM_COL32(80, 140, 240, 255);  // blue
                case scene::Axis::Z: return IM_COL32(80, 220, 120, 255);  // green
            }
            return IM_COL32(80, 220, 120, 255);
        }();
        const ImU32 axisColor = (mode == scene::SectionMode::SectionOnly)
                                    ? IM_COL32(255, 200, 64, 255)  // amber
                                    : perAxisColor;
        dl->AddLine(ImVec2{originPx.x, originPx.y},
                    ImVec2{grabPx.x, grabPx.y}, axisColor, 2.5F);
        dl->AddCircleFilled(ImVec2{originPx.x, originPx.y}, 3.5F, axisColor);
        const bool dragging = drag.active && drag.axis == axis;
        const ImU32 grabColor =
            dragging ? IM_COL32_WHITE : (grab.hovered ? IM_COL32(160, 255, 200, 255) : axisColor);
        dl->AddCircleFilled(ImVec2{grabPx.x, grabPx.y}, 9.0F, grabColor);
        dl->AddCircle(ImVec2{grabPx.x, grabPx.y}, 9.0F,
                      IM_COL32(0, 0, 0, 200), 0, 1.5F);
    }

    // --- Mode pills (B / F / C) — drawn as one glued segmented button ---
    auto modePill = [&](const char* label, glm::vec2 pos,
                        scene::SectionMode target,
                        ImDrawFlags cornerFlags) -> bool {
        std::snprintf(idBuf, sizeof(idBuf), "##asg_%s_%s", axisLetter, label);
        const float halfW = kPillWidth * 0.5F;
        const internal::RegionResult r = internal::HitRegion(
            idBuf, ImVec2{pos.x - halfW, pos.y - 10.0F},
            ImVec2{kPillWidth, 20.0F});
        const bool isActive = (mode == target);

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImU32 bg =
            isActive    ? IM_COL32(70, 160, 220, 255)
            : r.hovered ? IM_COL32(110, 110, 110, 255)
                        : IM_COL32(60, 60, 60, 255);
        dl->AddRectFilled(ImVec2{pos.x - halfW, pos.y - 10.0F},
                          ImVec2{pos.x + halfW, pos.y + 10.0F}, bg, 4.0F,
                          cornerFlags);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2{pos.x - ts.x * 0.5F, pos.y - ts.y * 0.5F},
                    IM_COL32_WHITE, label);

        if (r.pressed && !isActive) {
            mode = target;
            return true;
        }
        return false;
    };

    if (modePill("F", fPx, scene::SectionMode::CutFront,
                 ImDrawFlags_RoundCornersLeft)) {
        changed = true;
    }
    if (modePill("S", sPx, scene::SectionMode::SectionOnly,
                 ImDrawFlags_RoundCornersNone)) {
        changed = true;
    }
    if (modePill("B", bPx, scene::SectionMode::CutBack,
                 ImDrawFlags_RoundCornersRight)) {
        changed = true;
    }

    // Vertical separator lines between adjacent pills for visual segmentation.
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImU32 sep = IM_COL32(0, 0, 0, 96);
        for (int i = 1; i < 3; ++i) {
            const glm::vec2 c = groupStart + kHorizontal * (kPillWidth * i);
            dl->AddLine(ImVec2{c.x, c.y - 9.0F}, ImVec2{c.x, c.y + 9.0F}, sep,
                        1.0F);
        }
    }

    // --- Close button ---
    std::snprintf(idBuf, sizeof(idBuf), "##asg_close_%s", axisLetter);
    const internal::RegionResult closeBtn = internal::HitRegion(
        idBuf, ImVec2{xPx.x - 9.0F, xPx.y - 9.0F}, ImVec2{18.0F, 18.0F});
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImU32 bg = closeBtn.hovered ? IM_COL32(220, 70, 70, 255)
                                          : IM_COL32(120, 50, 50, 255);
        dl->AddCircleFilled(ImVec2{xPx.x, xPx.y}, 9.0F, bg);
        const ImVec2 ts = ImGui::CalcTextSize("x");
        // Nudge the glyph up ~1.5px: the lowercase 'x' has more visual mass
        // below its geometric centre, so pure half-height centring looks
        // optically low inside the circle.
        dl->AddText(ImVec2{xPx.x - ts.x * 0.5F, xPx.y - ts.y * 0.5F - 1.5F},
                    IM_COL32_WHITE, "x");
    }
    if (closeBtn.pressed) {
        open = false;
        changed = true;
    }

    return changed;
}

#endif  // BIMEUP_AXIS_SECTION_GIZMO_MATH_ONLY

}  // namespace bimeup::ui
