#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::renderer {
class ClipPlaneManager;
}  // namespace bimeup::renderer

namespace bimeup::scene {

class Scene;
class SceneMesh;

/// A single connected cross-section polyline produced by slicing one mesh (or
/// one batched-owner sub-mesh) against a plane and stitching the resulting
/// coplanar segments. Closed loops are the common case for watertight
/// geometry; IFC meshes routinely produce open polylines too, so both kinds
/// are preserved here — the cap builder triangulates closed loops (falling
/// back to open when nothing closes), and the edge builder emits every
/// polyline regardless.
struct SectionPolyline {
    std::vector<glm::vec3> points;  // ordered, world-space; closing edge implicit when closed
    bool closed = false;
    glm::vec4 ownerColor{1.0F};     // element colour (white for the test-only "attached" path)
};

/// All polylines produced by one `sectionFill`-enabled clip plane.
struct PerPlaneSectionPolylines {
    std::size_t planeIndex = 0;     // index into `manager.Planes()`
    glm::vec3 normal{0.0F};         // plane normal in world space
    glm::vec4 fillColor{1.0F};      // plane's section-fill tint
    std::vector<SectionPolyline> polylines;
};

/// Walks every visible mesh-bearing node (batched + attached paths) and runs
/// slice → stitch against each plane that has both `enabled` and `sectionFill`.
/// Shared producer for `BuildSectionCapVertices` and
/// `BuildSectionEdgeVertices`; both consumers dirty-track on the same state
/// hash computed by `ComputeSectionStateHash`.
std::vector<PerPlaneSectionPolylines> SliceAndStitchSectionPolylines(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager);

/// Hash of the subset of scene/mesh/manager state that affects the polyline
/// output. Used by the GPU-resident cap and edge geometry classes to skip
/// rebuilds when nothing relevant has changed.
std::size_t ComputeSectionStateHash(const Scene& scene,
                                    std::span<const SceneMesh> meshes,
                                    const renderer::ClipPlaneManager& manager);

}  // namespace bimeup::scene
