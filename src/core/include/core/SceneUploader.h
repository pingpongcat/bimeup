#pragma once

#include <renderer/MeshBuffer.h>
#include <scene/SceneBuilder.h>

namespace bimeup::core {

class SceneUploader {
public:
    /// Convert a SceneMesh to renderer-ready MeshData (pure data transform).
    static renderer::MeshData ToMeshData(const scene::SceneMesh& mesh);

    /// Upload all meshes from a BuildResult into the MeshBuffer.
    /// Updates each SceneNode's mesh handle from mesh-index to MeshBuffer handle.
    static void Upload(scene::BuildResult& result, renderer::MeshBuffer& meshBuffer);
};

} // namespace bimeup::core
