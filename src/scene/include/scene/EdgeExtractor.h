#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::scene {

struct EdgeExtractionConfig {
    /// Edges whose dihedral angle (between the two adjacent triangle normals)
    /// is at least this many degrees are kept as feature edges. Boundary
    /// edges (only one adjacent triangle) are always kept regardless.
    float dihedralAngleDegrees = 30.0f;

    /// Vertices whose positions are within this Euclidean distance are
    /// treated as the same vertex for topology purposes. This is how the
    /// extractor survives flat-shaded meshes (one vertex copy per face
    /// corner) that would otherwise report every edge as a boundary.
    float weldEpsilon = 1e-5f;
};

/// Line-list geometry: `indices.size()` is always even, each consecutive
/// pair is one line segment into `positions`.
struct ExtractedEdges {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
};

/// Build a feature-edge line list from a triangle mesh. Pure CPU, no Vulkan.
/// See `EdgeExtractionConfig` for the filter rules. Input `indices` length
/// must be a multiple of 3; non-multiples trigger no emit for the partial
/// triangle. Degenerate (zero-area) triangles are skipped.
ExtractedEdges ExtractFeatureEdges(const std::vector<glm::vec3>& positions,
                                   const std::vector<uint32_t>& indices,
                                   const EdgeExtractionConfig& config = {});

}  // namespace bimeup::scene
