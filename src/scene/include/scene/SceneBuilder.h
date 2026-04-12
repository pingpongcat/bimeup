#pragma once

#include "Scene.h"
#include "SceneMesh.h"

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
    BuildResult Build(ifc::IfcModel& model);
};

} // namespace bimeup::scene
