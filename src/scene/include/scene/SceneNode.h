#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "AABB.h"

namespace bimeup::scene {

using NodeId = uint32_t;
using MeshHandle = uint32_t;

inline constexpr NodeId InvalidNodeId = std::numeric_limits<NodeId>::max();

struct SceneNode {
    NodeId id = InvalidNodeId;
    uint32_t expressId = 0;  // IFC express id this scene node represents (0 = none)
    std::string name;
    std::string ifcType;
    std::string globalId;
    glm::mat4 transform = glm::mat4(1.0f);
    AABB bounds;
    std::optional<MeshHandle> mesh;
    NodeId parent = InvalidNodeId;
    std::vector<NodeId> children;
    bool visible = true;
    bool selected = false;
};

} // namespace bimeup::scene
