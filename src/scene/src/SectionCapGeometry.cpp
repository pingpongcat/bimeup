#include "scene/SectionCapGeometry.h"

#include <functional>
#include <unordered_map>

#include <renderer/Buffer.h>
#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>
#include <renderer/Device.h>

#include "scene/Scene.h"
#include "scene/SceneMesh.h"
#include "scene/SceneNode.h"
#include "scene/Slicing.h"

namespace bimeup::scene {

namespace {

void HashMix(std::size_t& h, std::size_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

std::size_t ComputeStateHash(const Scene& scene,
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

}  // namespace

namespace {

void AppendAttachedCaps(const renderer::ClipPlane& plane,
                       const glm::vec3& normal,
                       const SceneMesh& mesh,
                       const glm::mat4& transform,
                       std::vector<SectionVertex>& out) {
    const auto segments = SliceSceneMesh(mesh, transform, plane);
    if (segments.empty()) return;
    const auto loops = StitchSegments(segments);
    for (const auto& loop : loops) {
        const auto tris = TriangulatePolygon(loop, normal);
        out.reserve(out.size() + tris.size());
        for (const auto& v : tris) {
            out.push_back({v, plane.fillColor});
        }
    }
}

// Batched mesh: positions are already world-space, `triangleOwners[t]` names
// the scene node that each triangle belongs to. Slice+stitch per-owner so
// each element's cross-section is a self-contained loop — otherwise segments
// from different elements get tangled in the hash-grid walk and most caps
// drop. Per-owner slices are also often non-watertight (IFC meshes aren't
// guaranteed closed), so we fall back to the open polylines from
// `StitchSegmentsDetailed` when no closed loop forms.
void AppendBatchedCaps(const renderer::ClipPlane& plane,
                       const glm::vec3& normal,
                       const SceneMesh& mesh,
                       const Scene& scene,
                       std::vector<SectionVertex>& out) {
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

    std::fprintf(stderr,
                 "[section-cap] batched mesh: %zu unique owners with segments\n",
                 perOwnerSegs.size());

    for (auto& [owner, segs] : perOwnerSegs) {
        if (segs.empty()) continue;
        const auto colorIt = perOwnerColor.find(owner);
        const glm::vec4 ownerColor =
            colorIt != perOwnerColor.end() ? colorIt->second : glm::vec4(1.0F);
        // Tint = elementColor * planeFillColor (RGB), plane alpha. Preserves
        // per-element identity while honouring the plane's overall fill.
        const glm::vec4 tint(ownerColor.r * plane.fillColor.r,
                             ownerColor.g * plane.fillColor.g,
                             ownerColor.b * plane.fillColor.b,
                             plane.fillColor.a);

        const StitchResult stitched = StitchSegmentsDetailed(segs);
        auto emit = [&](const std::vector<glm::vec3>& loop) {
            const auto tris = TriangulatePolygon(loop, normal);
            out.reserve(out.size() + tris.size());
            for (const auto& v : tris) out.push_back({v, tint});
        };
        for (const auto& loop : stitched.closed) emit(loop);
        if (stitched.closed.empty()) {
            for (const auto& poly : stitched.open) emit(poly);
        }
    }
}

}  // namespace

std::vector<SectionVertex> BuildSectionCapVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager) {
    std::vector<SectionVertex> out;

    for (const auto& entry : manager.Planes()) {
        const auto& plane = entry.plane;
        if (!plane.enabled || !plane.sectionFill) continue;

        const glm::vec3 normal(plane.equation.x, plane.equation.y, plane.equation.z);

        // Batched path: iterate meshes directly. `triangleOwners` is the
        // authoritative per-triangle owner map and independent of whether a
        // scene node's `mesh` handle still correctly points here (SceneBuilder
        // batching can leave stale handles on non-batch-root nodes).
        for (const SceneMesh& mesh : meshes) {
            if (mesh.GetTriangleOwners().empty()) continue;
            AppendBatchedCaps(plane, normal, mesh, scene, out);
        }

        // Attached path (test-only in practice): meshes with no triangle
        // owners. Position data is local, so slicing needs the referencing
        // node's transform. Skip meshes already handled by the batched path.
        for (std::size_t i = 0; i < scene.GetNodeCount(); ++i) {
            const auto& node = scene.GetNode(static_cast<NodeId>(i));
            if (!node.visible || !node.mesh.has_value()) continue;
            const MeshHandle handle = node.mesh.value();
            if (handle >= meshes.size()) continue;
            const SceneMesh& mesh = meshes[handle];
            if (!mesh.GetTriangleOwners().empty()) continue;
            AppendAttachedCaps(plane, normal, mesh, node.transform, out);
        }
    }

    return out;
}

SectionCapGeometry::SectionCapGeometry(const renderer::Device& device)
    : device_(device) {}

SectionCapGeometry::~SectionCapGeometry() = default;

void SectionCapGeometry::Rebuild(const Scene& scene,
                                 std::span<const SceneMesh> meshes,
                                 const renderer::ClipPlaneManager& manager) {
    const std::size_t h = ComputeStateHash(scene, meshes, manager);
    if (!dirty_ && h == lastHash_) return;

    vertices_ = BuildSectionCapVertices(scene, meshes, manager);

    // Old vertex buffer may still be referenced by an in-flight command
    // buffer; wait for idle before replacing it (same pattern as MeshBuffer).
    if (vertexBuffer_ || !vertices_.empty()) {
        vkDeviceWaitIdle(device_.GetDevice());
    }

    if (!vertices_.empty()) {
        vertexBuffer_ = std::make_unique<renderer::Buffer>(
            device_, renderer::BufferType::Vertex,
            vertices_.size() * sizeof(SectionVertex),
            vertices_.data());
    } else {
        vertexBuffer_.reset();
    }

    lastHash_ = h;
    dirty_ = false;
}

void SectionCapGeometry::MarkDirty() { dirty_ = true; }

VkBuffer SectionCapGeometry::GetVertexBuffer() const {
    return vertexBuffer_ ? vertexBuffer_->GetBuffer() : VK_NULL_HANDLE;
}

}  // namespace bimeup::scene
