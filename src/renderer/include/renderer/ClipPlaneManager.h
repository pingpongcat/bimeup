#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "renderer/ClipPlane.h"

namespace bimeup::renderer {

struct ClipPlaneEntry {
    std::uint32_t id;
    ClipPlane plane;
};

class ClipPlaneManager {
public:
    static constexpr std::size_t kMaxPlanes = 6;
    static constexpr std::uint32_t kInvalidId = 0;

    std::uint32_t AddPlane(const glm::vec4& equation);
    bool RemovePlane(std::uint32_t id);
    bool SetEnabled(std::uint32_t id, bool enabled);
    bool UpdatePlane(std::uint32_t id, const glm::vec4& equation);

    std::size_t Count() const { return entries_.size(); }
    bool Contains(std::uint32_t id) const;
    const ClipPlane* Find(std::uint32_t id) const;
    const std::vector<ClipPlaneEntry>& Planes() const { return entries_; }

private:
    std::vector<ClipPlaneEntry> entries_;
    std::uint32_t nextId_{1};
};

}  // namespace bimeup::renderer
