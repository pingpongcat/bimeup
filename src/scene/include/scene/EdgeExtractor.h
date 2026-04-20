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

/// Build a feature-edge line list from a triangle mesh. Pure CPU, no Vulkan.
/// The returned vector is a flat line-list of indices into the *input*
/// `positions` array — consecutive pairs form one line segment. This lets
/// callers reuse the same vertex buffer for triangle and edge draws. When
/// the input was flat-shaded (multiple position copies per welded vertex),
/// the extractor picks the first input index that maps to each welded
/// vertex, so output indices are always valid subscripts into `positions`.
///
/// Rules (see `EdgeExtractionConfig`):
///   - boundary edges (one adjacent triangle) are always kept;
///   - edges whose dihedral angle ≥ `dihedralAngleDegrees` are kept;
///   - co-planar seams (triangles sharing an edge at < threshold) are dropped;
///   - degenerate (zero-area) triangles are skipped.
std::vector<uint32_t> ExtractFeatureEdges(const std::vector<glm::vec3>& positions,
                                          const std::vector<uint32_t>& indices,
                                          const EdgeExtractionConfig& config = {});

}  // namespace bimeup::scene
