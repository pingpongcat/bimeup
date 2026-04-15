#include "scene/SectionCapGeometry.h"

#include <functional>

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

std::vector<SectionVertex> BuildSectionCapVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager) {
    std::vector<SectionVertex> out;

    for (const auto& entry : manager.Planes()) {
        const auto& plane = entry.plane;
        if (!plane.enabled || !plane.sectionFill) continue;

        const glm::vec3 normal(plane.equation.x, plane.equation.y, plane.equation.z);

        for (std::size_t i = 0; i < scene.GetNodeCount(); ++i) {
            const auto& node = scene.GetNode(static_cast<NodeId>(i));
            if (!node.visible || !node.mesh.has_value()) continue;
            const MeshHandle handle = node.mesh.value();
            if (handle >= meshes.size()) continue;
            const SceneMesh& mesh = meshes[handle];

            auto segments = SliceSceneMesh(mesh, node.transform, plane);
            if (segments.empty()) continue;

            auto loops = StitchSegments(segments);
            for (const auto& loop : loops) {
                auto tris = TriangulatePolygon(loop, normal);
                out.reserve(out.size() + tris.size());
                for (const auto& v : tris) {
                    out.push_back({v, plane.fillColor});
                }
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
