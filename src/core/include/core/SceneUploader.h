#pragma once

#include <cstdint>

#include <renderer/MeshBuffer.h>
#include <scene/SceneBuilder.h>

namespace bimeup::renderer {
class DescriptorSet;
class TopLevelAS;
}

namespace bimeup::core {

class SceneUploader {
public:
    /// Convert a SceneMesh to renderer-ready MeshData (pure data transform).
    static renderer::MeshData ToMeshData(const scene::SceneMesh& mesh);

    /// Upload all meshes from a BuildResult into the MeshBuffer.
    /// Updates each SceneNode's mesh handle from mesh-index to MeshBuffer handle.
    static void Upload(scene::BuildResult& result, renderer::MeshBuffer& meshBuffer);

    /// Stage 9.Q.2 — write the scene's TLAS into `set` at `binding` so
    /// `basic.frag` can issue ray-query traces against it. No-op when the
    /// TLAS isn't valid (raster mode, RT-unavailable device, or scene with
    /// no instances): the descriptor binding stays unwritten and the
    /// shader's `useRayQueryShadow` push-constant gate (9.Q.3) keeps
    /// raster mode from sampling it.
    static void WriteTlasToDescriptor(renderer::DescriptorSet& set, uint32_t binding,
                                      const renderer::TopLevelAS& tlas);
};

} // namespace bimeup::core
