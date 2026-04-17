#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

namespace bimeup::renderer {
class ClipPlaneManager;
}

namespace bimeup::scene {

enum class Axis : std::uint8_t { X = 0, Y = 1, Z = 2 };

enum class SectionMode : std::uint8_t { CutFront, CutBack, SectionOnly };

struct AxisSectionSlot {
    float offset{0.0F};
    SectionMode mode{SectionMode::CutFront};
};

/// Plane equation for an axis-locked slot. CutFront keeps the +axis side at
/// `offset`; CutBack flips the normal so the −axis side is kept; SectionOnly
/// packs the same equation as CutFront (the "hide scene" behaviour is a
/// separate render-loop decision driven by `AnySectionOnly`).
glm::vec4 MakeAxisEquation(Axis axis, SectionMode mode, float offset);

/// Owns up to three axis-locked clip plane slots (one per X/Y/Z). The slots
/// are the source of truth; `SyncTo` mirrors them into a `ClipPlaneManager`
/// (adding, updating, or removing plane entries as needed) so the existing
/// renderer/shader path is reused without modification.
class AxisSectionController {
public:
    void SetSlot(Axis axis, AxisSectionSlot slot);
    void ClearSlot(Axis axis);

    [[nodiscard]] bool HasSlot(Axis axis) const;
    [[nodiscard]] std::optional<AxisSectionSlot> GetSlot(Axis axis) const;
    [[nodiscard]] std::size_t SlotCount() const;
    [[nodiscard]] bool AnySectionOnly() const;

    /// Idempotent — returns true iff the manager's plane state actually changed.
    /// After a successful sync, each active slot has exactly one plane entry
    /// in the manager with `sectionFill=true` and `enabled=true`.
    bool SyncTo(renderer::ClipPlaneManager& manager);

private:
    static constexpr std::size_t kAxisCount = 3;

    std::array<std::optional<AxisSectionSlot>, kAxisCount> slots_{};
    std::array<std::uint32_t, kAxisCount> ownedIds_{0, 0, 0};
};

}  // namespace bimeup::scene
