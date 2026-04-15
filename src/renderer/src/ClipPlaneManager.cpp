#include "renderer/ClipPlaneManager.h"

#include <algorithm>

namespace bimeup::renderer {

namespace {

auto FindEntry(std::vector<ClipPlaneEntry>& entries, std::uint32_t id) {
    return std::find_if(entries.begin(), entries.end(),
                        [id](const ClipPlaneEntry& e) { return e.id == id; });
}

auto FindEntry(const std::vector<ClipPlaneEntry>& entries, std::uint32_t id) {
    return std::find_if(entries.begin(), entries.end(),
                        [id](const ClipPlaneEntry& e) { return e.id == id; });
}

}  // namespace

std::uint32_t ClipPlaneManager::AddPlane(const glm::vec4& equation) {
    if (entries_.size() >= kMaxPlanes) return kInvalidId;
    const std::uint32_t id = nextId_++;
    entries_.push_back({id, ClipPlane{equation, true}});
    return id;
}

bool ClipPlaneManager::RemovePlane(std::uint32_t id) {
    if (id == kInvalidId) return false;
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return false;
    entries_.erase(it);
    return true;
}

bool ClipPlaneManager::SetEnabled(std::uint32_t id, bool enabled) {
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return false;
    it->plane.enabled = enabled;
    return true;
}

bool ClipPlaneManager::UpdatePlane(std::uint32_t id, const glm::vec4& equation) {
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return false;
    it->plane.equation = equation;
    return true;
}

bool ClipPlaneManager::SetSectionFill(std::uint32_t id, bool sectionFill) {
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return false;
    it->plane.sectionFill = sectionFill;
    return true;
}

bool ClipPlaneManager::SetFillColor(std::uint32_t id, const glm::vec4& color) {
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return false;
    it->plane.fillColor = color;
    return true;
}

bool ClipPlaneManager::Contains(std::uint32_t id) const {
    return FindEntry(entries_, id) != entries_.end();
}

const ClipPlane* ClipPlaneManager::Find(std::uint32_t id) const {
    auto it = FindEntry(entries_, id);
    if (it == entries_.end()) return nullptr;
    return &it->plane;
}

ClipPlanesUbo PackClipPlanes(const ClipPlaneManager& manager) {
    ClipPlanesUbo ubo{};
    int n = 0;
    for (const auto& entry : manager.Planes()) {
        if (!entry.plane.enabled) continue;
        ubo.planes[n] = entry.plane.equation;
        ++n;
    }
    ubo.count = glm::ivec4(n, 0, 0, 0);
    return ubo;
}

}  // namespace bimeup::renderer
