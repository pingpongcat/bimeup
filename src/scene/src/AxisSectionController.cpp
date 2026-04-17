#include "scene/AxisSectionController.h"

#include "renderer/ClipPlane.h"
#include "renderer/ClipPlaneManager.h"

namespace bimeup::scene {

namespace {

constexpr std::size_t AxisIndex(Axis axis) {
    return static_cast<std::size_t>(axis);
}

}  // namespace

glm::vec4 MakeAxisEquation(Axis axis, SectionMode mode, float offset) {
    const float sign = (mode == SectionMode::CutBack) ? -1.0F : 1.0F;
    glm::vec3 normal(0.0F);
    normal[static_cast<int>(axis)] = sign;
    const float d = -sign * offset;
    return glm::vec4(normal, d);
}

void AxisSectionController::SetSlot(Axis axis, AxisSectionSlot slot) {
    slots_[AxisIndex(axis)] = slot;
}

void AxisSectionController::ClearSlot(Axis axis) {
    slots_[AxisIndex(axis)].reset();
}

bool AxisSectionController::HasSlot(Axis axis) const {
    return slots_[AxisIndex(axis)].has_value();
}

std::optional<AxisSectionSlot> AxisSectionController::GetSlot(Axis axis) const {
    return slots_[AxisIndex(axis)];
}

std::size_t AxisSectionController::SlotCount() const {
    std::size_t n = 0;
    for (const auto& s : slots_) {
        if (s.has_value()) ++n;
    }
    return n;
}

bool AxisSectionController::AnySectionOnly() const {
    for (const auto& s : slots_) {
        if (s.has_value() && s->mode == SectionMode::SectionOnly) return true;
    }
    return false;
}

bool AxisSectionController::SyncTo(renderer::ClipPlaneManager& manager) {
    bool changed = false;

    for (std::size_t i = 0; i < kAxisCount; ++i) {
        const Axis axis = static_cast<Axis>(i);
        auto& slot = slots_[i];
        std::uint32_t& id = ownedIds_[i];

        if (slot.has_value()) {
            const glm::vec4 eq = MakeAxisEquation(axis, slot->mode, slot->offset);

            if (id == renderer::ClipPlaneManager::kInvalidId || !manager.Contains(id)) {
                id = manager.AddPlane(eq);
                if (id != renderer::ClipPlaneManager::kInvalidId) {
                    manager.SetSectionFill(id, true);
                    changed = true;
                }
            } else {
                const renderer::ClipPlane* existing = manager.Find(id);
                if (existing->equation != eq) {
                    manager.UpdatePlane(id, eq);
                    changed = true;
                }
                if (!existing->sectionFill) {
                    manager.SetSectionFill(id, true);
                    changed = true;
                }
                if (!existing->enabled) {
                    manager.SetEnabled(id, true);
                    changed = true;
                }
            }
        } else if (id != renderer::ClipPlaneManager::kInvalidId) {
            manager.RemovePlane(id);
            id = renderer::ClipPlaneManager::kInvalidId;
            changed = true;
        }
    }

    return changed;
}

}  // namespace bimeup::scene
