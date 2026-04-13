#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::scene {

enum class SnapType : std::uint8_t {
    None,
    Vertex,
    Edge,
    Face,
};

struct SnapResult {
    SnapType type{SnapType::None};
    glm::vec3 point{0.0f};
    float distance{0.0f};

    [[nodiscard]] bool IsValid() const { return type != SnapType::None; }
};

SnapResult SnapToVertex(const glm::vec3& query,
                        std::span<const glm::vec3> vertices,
                        float threshold);

SnapResult SnapToEdge(const glm::vec3& query,
                      std::span<const glm::vec3> vertices,
                      std::span<const std::uint32_t> indices,
                      float threshold);

SnapResult SnapToFace(const glm::vec3& query,
                      std::span<const glm::vec3> vertices,
                      std::span<const std::uint32_t> indices,
                      float threshold);

/// Combined snap: returns the highest-priority hit (Vertex > Edge > Face)
/// whose distance is within its respective threshold.
SnapResult Snap(const glm::vec3& query,
                std::span<const glm::vec3> vertices,
                std::span<const std::uint32_t> indices,
                float vertexThreshold,
                float edgeThreshold,
                float faceThreshold);

} // namespace bimeup::scene
