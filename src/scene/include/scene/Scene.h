#pragma once

#include "SceneNode.h"

#include <stdexcept>
#include <string>
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
    /// Set visibility for every node whose ifcType exactly matches @p ifcType.
    /// Returns number of nodes affected.
    size_t SetVisibilityByType(const std::string& ifcType, bool visible);
    void SetSelected(NodeId id, bool selected);
    std::vector<NodeId> GetSelected() const;
    std::vector<NodeId> FindByType(const std::string& ifcType) const;
    /// Sorted, de-duplicated list of non-empty ifcType values present in the scene.
    std::vector<std::string> GetUniqueTypes() const;

private:
    std::vector<SceneNode> nodes_;
    NodeId nextId_ = 0;
};

/// IFC types that represent non-visual concepts (spatial volumes, openings, grids)
/// and should be hidden by default on model load.
const std::vector<std::string>& DefaultHiddenTypes();

} // namespace bimeup::scene
