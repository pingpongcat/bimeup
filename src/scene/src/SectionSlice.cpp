#include "scene/SectionSlice.h"

#include <functional>
#include <unordered_map>

#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>

#include "scene/Scene.h"
#include "scene/SceneMesh.h"
#include "scene/SceneNode.h"
#include "scene/Slicing.h"

namespace bimeup::scene {

namespace {

void HashMix(std::size_t& h, std::size_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

void AppendAttachedPolylines(const renderer::ClipPlane& plane,
                             const SceneMesh& mesh,
                             const glm::mat4& transform,
                             std::vector<SectionPolyline>& out) {
    const auto segments = SliceSceneMesh(mesh, transform, plane);
    if (segments.empty()) return;
    const auto loops = StitchSegments(segments);
    for (const auto& loop : loops) {
        out.push_back({loop, /*closed=*/true, glm::vec4(1.0F)});
    }
}

// Batched mesh: positions are already world-space, `triangleOwners[t]` names
// the scene node each triangle belongs to. Slice+stitch per owner so each
// element's cross-section stays self-contained; otherwise segments from
// different elements collide in the hash-grid walk and most outlines drop.
void AppendBatchedPolylines(const renderer::ClipPlane& plane,
                            const SceneMesh& mesh,
                            const Scene& scene,
                            std::vector<SectionPolyline>& out) {
    const auto& owners = mesh.GetTriangleOwners();
    const auto& indices = mesh.GetIndices();
    const auto& positions = mesh.GetPositions();
    const auto& colors = mesh.GetColors();
    if (owners.empty() || indices.size() < owners.size() * 3) return;

    std::unordered_map<NodeId, std::vector<Segment>> perOwnerSegs;
    std::unordered_map<NodeId, glm::vec4> perOwnerColor;

    for (std::size_t t = 0; t < owners.size(); ++t) {
        const NodeId owner = owners[t];
        if (owner == InvalidNodeId) continue;
        if (owner >= scene.GetNodeCount()) continue;
        if (!scene.GetNode(owner).visible) continue;

        const std::uint32_t ia = indices[3 * t + 0];
        const std::uint32_t ib = indices[3 * t + 1];
        const std::uint32_t ic = indices[3 * t + 2];
        if (ia >= positions.size() || ib >= positions.size() ||
            ic >= positions.size()) {
            continue;
        }

        const TriangleCut cut = SliceTriangle(plane, positions[ia], positions[ib],
                                              positions[ic]);
        if (cut.pointCount == 2) {
            perOwnerSegs[owner].push_back({cut.points[0], cut.points[1]});
        }

        if (ia < colors.size() &&
            perOwnerColor.find(owner) == perOwnerColor.end()) {
            perOwnerColor[owner] = colors[ia];
        }
    }

    for (auto& [owner, segs] : perOwnerSegs) {
        if (segs.empty()) continue;
        const auto colorIt = perOwnerColor.find(owner);
        const glm::vec4 ownerColor =
            colorIt != perOwnerColor.end() ? colorIt->second : glm::vec4(1.0F);

        const StitchResult stitched = StitchSegmentsDetailed(segs);
        for (const auto& loop : stitched.closed) {
            out.push_back({loop, /*closed=*/true, ownerColor});
        }
        // Fallback to open polylines only when no loop closed — matches the
        // cap-triangulation invariant so caps + outlines stay in agreement.
        if (stitched.closed.empty()) {
            for (const auto& poly : stitched.open) {
                out.push_back({poly, /*closed=*/false, ownerColor});
            }
        }
    }
}

}  // namespace

std::vector<PerPlaneSectionPolylines> SliceAndStitchSectionPolylines(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager) {
    std::vector<PerPlaneSectionPolylines> out;

    std::size_t planeIndex = 0;
    for (const auto& entry : manager.Planes()) {
        const auto& plane = entry.plane;
        if (!plane.enabled || !plane.sectionFill) {
            ++planeIndex;
            continue;
        }

        PerPlaneSectionPolylines perPlane{};
        perPlane.planeIndex = planeIndex;
        perPlane.normal = glm::vec3(plane.equation.x, plane.equation.y, plane.equation.z);
        perPlane.fillColor = plane.fillColor;

        // Batched path — authoritative per-triangle owner map.
        for (const SceneMesh& mesh : meshes) {
            if (mesh.GetTriangleOwners().empty()) continue;
            AppendBatchedPolylines(plane, mesh, scene, perPlane.polylines);
        }

        // Attached path — meshes with no triangle owners (test-only in practice).
        for (std::size_t i = 0; i < scene.GetNodeCount(); ++i) {
            const auto& node = scene.GetNode(static_cast<NodeId>(i));
            if (!node.visible || !node.mesh.has_value()) continue;
            const MeshHandle handle = node.mesh.value();
            if (handle >= meshes.size()) continue;
            const SceneMesh& mesh = meshes[handle];
            if (!mesh.GetTriangleOwners().empty()) continue;
            AppendAttachedPolylines(plane, mesh, node.transform,
                                    perPlane.polylines);
        }

        out.push_back(std::move(perPlane));
        ++planeIndex;
    }

    return out;
}

std::size_t ComputeSectionStateHash(const Scene& scene,
                                    std::span<const SceneMesh> meshes,
                                    const renderer::ClipPlaneManager& manager) {
    std::size_t h = 0;
    HashMix(h, std::hash<std::size_t>{}(scene.GetNodeCount()));
    HashMix(h, std::hash<std::size_t>{}(meshes.size()));
    const auto hf = std::hash<float>{};
    const auto hb = std::hash<int>{};
    for (const auto& entry : manager.Planes()) {
        const auto& p = entry.plane;
        HashMix(h, hf(p.equation.x));
        HashMix(h, hf(p.equation.y));
        HashMix(h, hf(p.equation.z));
        HashMix(h, hf(p.equation.w));
        HashMix(h, hb(p.enabled ? 1 : 0));
        HashMix(h, hb(p.sectionFill ? 1 : 0));
        HashMix(h, hf(p.fillColor.r));
        HashMix(h, hf(p.fillColor.g));
        HashMix(h, hf(p.fillColor.b));
        HashMix(h, hf(p.fillColor.a));
    }
    return h;
}

}  // namespace bimeup::scene
