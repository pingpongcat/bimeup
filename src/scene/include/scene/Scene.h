#pragma once

#include "SceneNode.h"

#include <stdexcept>
#include <vector>

namespace bimeup::scene {

class Scene {
public:
    NodeId AddNode(SceneNode node);
    SceneNode& GetNode(NodeId id);
    const SceneNode& GetNode(NodeId id) const;
    std::vector<NodeId> GetRoots() const;
    std::vector<NodeId> GetChildren(NodeId id) const;
    size_t GetNodeCount() const;
    void SetVisibility(NodeId id, bool visible, bool recursive = false);
    void SetSelected(NodeId id, bool selected);
    std::vector<NodeId> GetSelected() const;
    std::vector<NodeId> FindByType(const std::string& ifcType) const;

private:
    std::vector<SceneNode> nodes_;
    NodeId nextId_ = 0;
};

} // namespace bimeup::scene
