#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "renderer/ClipPlane.h"

namespace bimeup::scene {

struct TriangleCut {
    std::uint8_t pointCount{0};        // 0 or 2 (single-point touches are dropped)
    std::array<glm::vec3, 2> points{};  // world-space intersection points
};

TriangleCut SliceTriangle(const renderer::ClipPlane& plane,
                          const glm::vec3& a,
                          const glm::vec3& b,
                          const glm::vec3& c);

}  // namespace bimeup::scene
