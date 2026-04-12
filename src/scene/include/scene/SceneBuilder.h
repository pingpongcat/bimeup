#pragma once

#include "Scene.h"
#include "SceneMesh.h"

#include <cstddef>
#include <vector>

namespace bimeup::ifc {
class IfcModel;
}

namespace bimeup::scene {

struct BuildResult {
    Scene scene;
    std::vector<SceneMesh> meshes;
};

class SceneBuilder {
public:
    void SetBatchingEnabled(bool enabled) { batchingEnabled_ = enabled; }
    void SetBatchThreshold(size_t maxVertices) { batchThreshold_ = maxVertices; }

    BuildResult Build(ifc::IfcModel& model);

    /// Merge small meshes that share the same IFC type and color into combined
    /// meshes, reducing draw call count. Meshes with more vertices than
    /// @p maxVertices are kept individual.
    static void ApplyBatching(BuildResult& result, size_t maxVertices = 1024);

private:
    bool batchingEnabled_ = false;
    size_t batchThreshold_ = 1024;
};

} // namespace bimeup::scene
