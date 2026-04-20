#include "scene/SectionCapGeometry.h"

#include <renderer/Buffer.h>
#include <renderer/Device.h>

#include "scene/Scene.h"
#include "scene/SceneMesh.h"
#include "scene/SectionSlice.h"
#include "scene/Slicing.h"

namespace bimeup::scene {

std::vector<SectionVertex> BuildSectionCapVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager) {
    std::vector<SectionVertex> out;

    const auto perPlane = SliceAndStitchSectionPolylines(scene, meshes, manager);
    for (const auto& plane : perPlane) {
        // Cap invariant: when any closed loop formed for this plane we trust
        // the triangulation; only fall back to open polylines for a given
        // owner when nothing closed. The shared producer already enforces that
        // per-owner, so every polyline here is ready to ear-clip.
        for (const auto& poly : plane.polylines) {
            if (poly.points.size() < 3) continue;
            // Tint = elementColor * planeFillColor (RGB), plane alpha.
            const glm::vec4 tint(poly.ownerColor.r * plane.fillColor.r,
                                 poly.ownerColor.g * plane.fillColor.g,
                                 poly.ownerColor.b * plane.fillColor.b,
                                 plane.fillColor.a);
            const auto tris = TriangulatePolygon(poly.points, plane.normal);
            out.reserve(out.size() + tris.size());
            for (const auto& v : tris) {
                out.push_back({v, tint});
            }
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
    const std::size_t h = ComputeSectionStateHash(scene, meshes, manager);
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
