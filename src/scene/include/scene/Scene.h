#pragma once

#include "SceneNode.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
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
    /// Isolate a set of elements by IFC expressId: every mesh-bearing node
    /// becomes visible iff its expressId is in @p expressIds. Non-mesh nodes
    /// (spatial containers, groups) are left untouched. Returns number of
    /// mesh-bearing nodes whose visibility actually changed.
    size_t IsolateByExpressId(const std::unordered_set<std::uint32_t>& expressIds);
    /// Convenience overload for small lists / initializer lists.
    size_t IsolateByExpressId(std::initializer_list<std::uint32_t> expressIds);
    /// Force every node visible. Inverse of any Isolate/SetVisibilityByType call.
    void ShowAll();
    void SetSelected(NodeId id, bool selected);
    std::vector<NodeId> GetSelected() const;
    std::vector<NodeId> FindByType(const std::string& ifcType) const;
    /// Scene nodes whose expressId equals @p expressId (typically 0 or 1 match).
    std::vector<NodeId> FindByExpressId(std::uint32_t expressId) const;
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
