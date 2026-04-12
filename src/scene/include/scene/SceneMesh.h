#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::scene {

class SceneMesh {
public:
    void SetPositions(std::vector<glm::vec3> positions);
    void SetNormals(std::vector<glm::vec3> normals);
    void SetColors(std::vector<glm::vec4> colors);
    void SetIndices(std::vector<uint32_t> indices);

    /// Set all vertex colors to a single uniform color (resizes to match position count).
    void SetUniformColor(const glm::vec4& color);

    [[nodiscard]] const std::vector<glm::vec3>& GetPositions() const { return positions_; }
    [[nodiscard]] const std::vector<glm::vec3>& GetNormals() const { return normals_; }
    [[nodiscard]] const std::vector<glm::vec4>& GetColors() const { return colors_; }
    [[nodiscard]] const std::vector<uint32_t>& GetIndices() const { return indices_; }

    [[nodiscard]] size_t GetVertexCount() const { return positions_.size(); }
    [[nodiscard]] size_t GetIndexCount() const { return indices_.size(); }

    [[nodiscard]] std::vector<float> GetInterleavedVertices() const;

    /// Vertex stride in bytes: 3 + 3 + 4 = 10 floats = 40 bytes.
    static constexpr size_t VertexStrideBytes() { return 10 * sizeof(float); }

private:
    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> normals_;
    std::vector<glm::vec4> colors_;
    std::vector<uint32_t> indices_;
};

} // namespace bimeup::scene
