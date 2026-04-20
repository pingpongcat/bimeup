#include "scene/SectionEdgeGeometry.h"

#include <renderer/Buffer.h>
#include <renderer/Device.h>

#include "scene/Scene.h"
#include "scene/SceneMesh.h"
#include "scene/SectionSlice.h"

namespace bimeup::scene {

namespace {

void EmitPolylineAsLineList(const SectionPolyline& poly,
                            std::vector<SectionEdgeVertex>& out) {
    const auto& pts = poly.points;
    if (pts.size() < 2) return;
    const std::size_t segCount = poly.closed ? pts.size() : pts.size() - 1;
    out.reserve(out.size() + segCount * 2);
    for (std::size_t i = 0; i < segCount; ++i) {
        const std::size_t next = (i + 1) % pts.size();
        SectionEdgeVertex a{};
        SectionEdgeVertex b{};
        a.position = pts[i];
        b.position = pts[next];
        out.push_back(a);
        out.push_back(b);
    }
}

}  // namespace

std::vector<SectionEdgeVertex> BuildSectionEdgeVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager) {
    std::vector<SectionEdgeVertex> out;

    const auto perPlane = SliceAndStitchSectionPolylines(scene, meshes, manager);
    for (const auto& plane : perPlane) {
        for (const auto& poly : plane.polylines) {
            EmitPolylineAsLineList(poly, out);
        }
    }

    return out;
}

SectionEdgeGeometry::SectionEdgeGeometry(const renderer::Device& device)
    : device_(device) {}

SectionEdgeGeometry::~SectionEdgeGeometry() = default;

void SectionEdgeGeometry::Rebuild(const Scene& scene,
                                  std::span<const SceneMesh> meshes,
                                  const renderer::ClipPlaneManager& manager) {
    const std::size_t h = ComputeSectionStateHash(scene, meshes, manager);
    if (!dirty_ && h == lastHash_) return;

    vertices_ = BuildSectionEdgeVertices(scene, meshes, manager);

    if (vertexBuffer_ || !vertices_.empty()) {
        vkDeviceWaitIdle(device_.GetDevice());
    }

    if (!vertices_.empty()) {
        vertexBuffer_ = std::make_unique<renderer::Buffer>(
            device_, renderer::BufferType::Vertex,
            vertices_.size() * sizeof(SectionEdgeVertex),
            vertices_.data());
    } else {
        vertexBuffer_.reset();
    }

    lastHash_ = h;
    dirty_ = false;
}

void SectionEdgeGeometry::MarkDirty() { dirty_ = true; }

VkBuffer SectionEdgeGeometry::GetVertexBuffer() const {
    return vertexBuffer_ ? vertexBuffer_->GetBuffer() : VK_NULL_HANDLE;
}

}  // namespace bimeup::scene
