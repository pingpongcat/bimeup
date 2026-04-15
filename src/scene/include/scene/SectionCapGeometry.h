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

struct SectionVertex {
    glm::vec3 position;
    glm::vec4 color;
};

/// Pure-CPU build of section cap triangles. For each plane with `sectionFill`
/// and `enabled`, walks every visible mesh-bearing node, slices its triangles
/// against the plane, stitches the resulting segments into closed polygons,
/// triangulates them, and tints every vertex with the plane's `fillColor`.
/// Output vertex count is always a multiple of 3.
std::vector<SectionVertex> BuildSectionCapVertices(
    const Scene& scene,
    std::span<const SceneMesh> meshes,
    const renderer::ClipPlaneManager& manager);

/// GPU-resident cap geometry. `Rebuild` hashes the relevant (plane+scene)
/// state; if the hash matches the previous call, it's a no-op. `MarkDirty`
/// forces the next `Rebuild` to run regardless (use after scene reload).
class SectionCapGeometry {
public:
    explicit SectionCapGeometry(const renderer::Device& device);
    ~SectionCapGeometry();

    SectionCapGeometry(const SectionCapGeometry&) = delete;
    SectionCapGeometry& operator=(const SectionCapGeometry&) = delete;
    SectionCapGeometry(SectionCapGeometry&&) = delete;
    SectionCapGeometry& operator=(SectionCapGeometry&&) = delete;

    void Rebuild(const Scene& scene,
                 std::span<const SceneMesh> meshes,
                 const renderer::ClipPlaneManager& manager);

    void MarkDirty();

    [[nodiscard]] VkBuffer GetVertexBuffer() const;
    [[nodiscard]] std::uint32_t GetVertexCount() const {
        return static_cast<std::uint32_t>(vertices_.size());
    }
    [[nodiscard]] bool IsEmpty() const { return vertices_.empty(); }
    [[nodiscard]] const std::vector<SectionVertex>& GetVertices() const {
        return vertices_;
    }

private:
    const renderer::Device& device_;
    std::vector<SectionVertex> vertices_;
    std::unique_ptr<renderer::Buffer> vertexBuffer_;
    std::size_t lastHash_ = 0;
    bool dirty_ = true;
};

}  // namespace bimeup::scene
