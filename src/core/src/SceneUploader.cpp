#include <core/SceneUploader.h>

#include <scene/SceneNode.h>

namespace bimeup::core {

renderer::MeshData SceneUploader::ToMeshData(const scene::SceneMesh& mesh) {
    renderer::MeshData data;

    const auto& positions = mesh.GetPositions();
    const auto& normals = mesh.GetNormals();
    const auto& colors = mesh.GetColors();

    data.vertices.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        data.vertices[i].position = positions[i];
        data.vertices[i].normal = i < normals.size() ? normals[i] : glm::vec3(0.0F, 1.0F, 0.0F);
        data.vertices[i].color = i < colors.size() ? colors[i] : glm::vec4(0.8F, 0.8F, 0.8F, 1.0F);
    }

    data.indices = mesh.GetIndices();
    data.edgeIndices = mesh.GetEdgeIndices();
    return data;
}

void SceneUploader::Upload(scene::BuildResult& result, renderer::MeshBuffer& meshBuffer) {
    // Upload each SceneMesh and build a mapping from mesh-index to MeshBuffer handle
    std::vector<renderer::MeshHandle> handleMap(result.meshes.size(), renderer::MeshBuffer::InvalidHandle);

    for (size_t i = 0; i < result.meshes.size(); ++i) {
        if (result.meshes[i].GetVertexCount() == 0) {
            continue;
        }
        handleMap[i] = meshBuffer.Upload(ToMeshData(result.meshes[i]));
    }

    // Update SceneNode mesh handles from mesh-index to MeshBuffer handle
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        auto& node = result.scene.GetNode(static_cast<scene::NodeId>(i));
        if (!node.mesh.has_value()) {
            continue;
        }

        auto meshIndex = node.mesh.value();
        if (meshIndex < handleMap.size() && handleMap[meshIndex] != renderer::MeshBuffer::InvalidHandle) {
            node.mesh = handleMap[meshIndex];
        } else {
            node.mesh.reset();
        }
    }
}

} // namespace bimeup::core
