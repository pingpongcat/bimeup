#include <scene/SceneMesh.h>

namespace bimeup::scene {

void SceneMesh::SetPositions(std::vector<glm::vec3> positions) {
    positions_ = std::move(positions);
}

void SceneMesh::SetNormals(std::vector<glm::vec3> normals) {
    normals_ = std::move(normals);
}

void SceneMesh::SetColors(std::vector<glm::vec4> colors) {
    colors_ = std::move(colors);
}

void SceneMesh::SetIndices(std::vector<uint32_t> indices) {
    indices_ = std::move(indices);
}

void SceneMesh::SetUniformColor(const glm::vec4& color) {
    colors_.assign(positions_.size(), color);
}

std::vector<float> SceneMesh::GetInterleavedVertices() const {
    if (positions_.empty()) {
        return {};
    }

    const size_t vertexCount = positions_.size();
    std::vector<float> result;
    result.reserve(vertexCount * 10);

    for (size_t i = 0; i < vertexCount; ++i) {
        // Position (vec3)
        result.push_back(positions_[i].x);
        result.push_back(positions_[i].y);
        result.push_back(positions_[i].z);

        // Normal (vec3) — default to (0,0,0) if normals not provided
        if (i < normals_.size()) {
            result.push_back(normals_[i].x);
            result.push_back(normals_[i].y);
            result.push_back(normals_[i].z);
        } else {
            result.push_back(0.0f);
            result.push_back(0.0f);
            result.push_back(0.0f);
        }

        // Color (vec4) — default to white if colors not provided
        if (i < colors_.size()) {
            result.push_back(colors_[i].r);
            result.push_back(colors_[i].g);
            result.push_back(colors_[i].b);
            result.push_back(colors_[i].a);
        } else {
            result.push_back(1.0f);
            result.push_back(1.0f);
            result.push_back(1.0f);
            result.push_back(1.0f);
        }
    }

    return result;
}

} // namespace bimeup::scene
