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
    /// RP.17 — feature-edge line-list (pairs of indices into `vertices`).
    /// Empty for meshes without an extracted edge overlay.
    std::vector<uint32_t> edgeIndices;
};

using MeshHandle = uint32_t;

struct DrawParams {
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    /// RP.17 — edge-overlay draw params (line-list, shares `vertexOffset`
    /// with the triangle draw above). `edgeIndexCount == 0` means the mesh
    /// has no extracted edges and the edge overlay pass should skip it.
    uint32_t edgeIndexCount = 0;
    uint32_t firstEdgeIndex = 0;
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

    /// RP.17 — bind the vertex buffer and the line-list edge-index buffer
    /// for the edge-overlay pass. No-op when no mesh has edges uploaded.
    void BindEdges(VkCommandBuffer cmd) const;
    /// RP.17 — draw the edge overlay for a mesh. No-op when the mesh has
    /// no extracted edges.
    void DrawEdges(VkCommandBuffer cmd, MeshHandle handle) const;
    /// RP.17 — true when at least one uploaded mesh has extracted edges.
    /// Callers can skip binding + iterating when this is false.
    [[nodiscard]] bool HasEdges() const { return !m_edgeIndices.empty(); }

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
    // RP.17 — line-list of (vertex, vertex) pairs for the edge overlay.
    // Per-handle ranges tracked by `DrawParams::firstEdgeIndex/edgeIndexCount`.
    std::vector<uint32_t> m_edgeIndices;
    std::unordered_map<MeshHandle, DrawParams> m_meshes;

    // Override layers (applied in order: baseline -> alpha -> color).
    std::unordered_map<uint32_t, float> m_alphaOverrides;
    std::vector<uint32_t> m_colorOverrideIndices;
    glm::vec4 m_colorOverrideColor{0.0F};

    // GPU buffers
    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;
    std::unique_ptr<Buffer> m_edgeIndexBuffer;  // RP.17 — line-list index buffer
    bool m_dirty = false;
};

}  // namespace bimeup::renderer
