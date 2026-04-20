#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "SceneNode.h"

namespace bimeup::scene {

class SceneMesh {
public:
    void SetPositions(std::vector<glm::vec3> positions);
    void SetNormals(std::vector<glm::vec3> normals);
    void SetColors(std::vector<glm::vec4> colors);
    void SetIndices(std::vector<uint32_t> indices);

    /// Feature-edge line list (pairs of indices into `positions_`). Produced
    /// by `scene::ExtractFeatureEdges` at scene-build time; consumed by the
    /// renderer's edge-overlay pass. Empty when the mesh has no extractable
    /// edges or extraction has not run.
    void SetEdgeIndices(std::vector<uint32_t> edgeIndices);

    /// Set all vertex colors to a single uniform color (resizes to match position count).
    void SetUniformColor(const glm::vec4& color);

    /// Per-triangle owning NodeId for this mesh (length = index_count / 3).
    /// When empty, the mesh is treated as "attached" (picked via the node that
    /// references it with its transform); when populated, positions are assumed
    /// to be world-space and ownership is resolved per triangle.
    void SetTriangleOwners(std::vector<NodeId> owners);

    [[nodiscard]] const std::vector<glm::vec3>& GetPositions() const { return positions_; }
    [[nodiscard]] const std::vector<glm::vec3>& GetNormals() const { return normals_; }
    [[nodiscard]] const std::vector<glm::vec4>& GetColors() const { return colors_; }
    [[nodiscard]] const std::vector<uint32_t>& GetIndices() const { return indices_; }
    [[nodiscard]] const std::vector<uint32_t>& GetEdgeIndices() const { return edgeIndices_; }
    [[nodiscard]] const std::vector<NodeId>& GetTriangleOwners() const { return triangleOwners_; }

    [[nodiscard]] size_t GetVertexCount() const { return positions_.size(); }
    [[nodiscard]] size_t GetIndexCount() const { return indices_.size(); }
    [[nodiscard]] size_t GetEdgeIndexCount() const { return edgeIndices_.size(); }

    /// True if the first vertex color has alpha < 1 (uniform-colored meshes
    /// only — mixed-alpha vertex buffers are not produced by the builder).
    /// Used as an opacity bucket for batching and later alpha-blended pass.
    [[nodiscard]] bool IsTransparent() const {
        return !colors_.empty() && colors_[0].a < 0.999f;
    }

    [[nodiscard]] std::vector<float> GetInterleavedVertices() const;

    /// Vertex stride in bytes: 3 + 3 + 4 = 10 floats = 40 bytes.
    static constexpr size_t VertexStrideBytes() { return 10 * sizeof(float); }

private:
    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> normals_;
    std::vector<glm::vec4> colors_;
    std::vector<uint32_t> indices_;
    std::vector<uint32_t> edgeIndices_;
    std::vector<NodeId> triangleOwners_;
};

} // namespace bimeup::scene
