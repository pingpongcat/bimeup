#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace bimeup::renderer {
class Buffer;
class ClipPlaneManager;
class Device;
}  // namespace bimeup::renderer

namespace bimeup::scene {

class Scene;
class SceneMesh;

/// Vertex layout matching `renderer::Vertex` (40 B stride: position + normal +
/// colour) so the existing `EdgeOverlayPipeline` — which binds a 40-B stride
/// and samples only the position attribute — can draw section outlines without
/// a dedicated variant. Normal and colour are left zero; the pipeline ignores
/// them and supplies the edge tint via push-constant.
struct SectionEdgeVertex {
    glm::vec3 position;
    glm::vec3 normal{0.0F};
    glm::vec4 color{0.0F};
};

/// Pure-CPU build of section outline line-list vertices. For each plane with
/// `sectionFill` and `enabled`, reuses the shared slice+stitch pass
/// (`SliceAndStitchSectionPolylines`) and emits one line-list segment per
/// polyline edge (closing edge included for closed loops). Output size is
/// always a multiple of 2.
std::vector<SectionEdgeVertex> BuildSectionEdgeVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager);

/// GPU-resident section-outline geometry. Mirrors `SectionCapGeometry`'s
/// dirty-track on the shared `ComputeSectionStateHash` so both rebuild at
/// the same cadence.
class SectionEdgeGeometry {
public:
    explicit SectionEdgeGeometry(const renderer::Device& device);
    ~SectionEdgeGeometry();

    SectionEdgeGeometry(const SectionEdgeGeometry&) = delete;
    SectionEdgeGeometry& operator=(const SectionEdgeGeometry&) = delete;
    SectionEdgeGeometry(SectionEdgeGeometry&&) = delete;
    SectionEdgeGeometry& operator=(SectionEdgeGeometry&&) = delete;

    void Rebuild(const Scene& scene,
                 std::span<const SceneMesh> meshes,
                 const renderer::ClipPlaneManager& manager);

    void MarkDirty();

    [[nodiscard]] VkBuffer GetVertexBuffer() const;
    [[nodiscard]] std::uint32_t GetVertexCount() const {
        return static_cast<std::uint32_t>(vertices_.size());
    }
    [[nodiscard]] bool IsEmpty() const { return vertices_.empty(); }
    [[nodiscard]] const std::vector<SectionEdgeVertex>& GetVertices() const {
        return vertices_;
    }

private:
    const renderer::Device& device_;
    std::vector<SectionEdgeVertex> vertices_;
    std::unique_ptr<renderer::Buffer> vertexBuffer_;
    std::size_t lastHash_ = 0;
    bool dirty_ = true;
};

}  // namespace bimeup::scene
