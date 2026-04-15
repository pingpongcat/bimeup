#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace bimeup::renderer {

class Buffer;
class Device;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

using MeshHandle = uint32_t;

struct DrawParams {
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
};

class MeshBuffer {
public:
    static constexpr MeshHandle InvalidHandle = 0;

    explicit MeshBuffer(const Device& device);
    ~MeshBuffer();

    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;
    MeshBuffer(MeshBuffer&&) = delete;
    MeshBuffer& operator=(MeshBuffer&&) = delete;

    MeshHandle Upload(const MeshData& data);
    void Remove(MeshHandle handle);

    [[nodiscard]] bool IsValid(MeshHandle handle) const;
    [[nodiscard]] DrawParams GetDrawParams(MeshHandle handle) const;
    [[nodiscard]] std::size_t MeshCount() const;

    void Bind(VkCommandBuffer cmd) const;
    void Draw(VkCommandBuffer cmd, MeshHandle handle) const;

    /// Replace the per-vertex color of the given global vertex indices with `color`,
    /// restoring all other vertices to their originally uploaded colors. Pass an
    /// empty `indices` to clear any previous override.
    void SetVertexColorOverride(const std::vector<uint32_t>& indices, glm::vec4 color);

    /// Force per-vertex alpha (preserving baseline RGB) for the listed
    /// (vertex_index, alpha) pairs. Alpha is clamped to [0,1]. Pass an empty
    /// list to clear. Layered below any active color override — vertices hit
    /// by both layers keep the color override's full RGBA.
    void SetVertexAlphaOverride(const std::vector<std::pair<uint32_t, float>>& alphas);

    [[nodiscard]] const std::vector<Vertex>& GetVerticesForTesting() const { return m_vertices; }

private:
    void RebuildGpuBuffers();
    void RecomputeVertexColors();

    const Device& m_device;
    MeshHandle m_nextHandle = 1;

    // CPU-side staging
    std::vector<Vertex> m_vertices;
    std::vector<glm::vec4> m_baselineColors;  // original colors from Upload, parallel to m_vertices
    std::vector<uint32_t> m_indices;
    std::unordered_map<MeshHandle, DrawParams> m_meshes;

    // Override layers (applied in order: baseline -> alpha -> color).
    std::unordered_map<uint32_t, float> m_alphaOverrides;
    std::vector<uint32_t> m_colorOverrideIndices;
    glm::vec4 m_colorOverrideColor{0.0F};

    // GPU buffers
    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;
    bool m_dirty = false;
};

}  // namespace bimeup::renderer
