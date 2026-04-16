#pragma once

#include "SceneNode.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

    /// Per-element alpha override, keyed by IFC expressId. Value is clamped to
    /// [0,1]. Forces transparency on top of any IFC-native alpha. Element
    /// override wins over any type-level override for the same node.
    void SetElementAlphaOverride(std::uint32_t expressId, float alpha);
    void ClearElementAlphaOverride(std::uint32_t expressId);
    [[nodiscard]] std::optional<float> GetElementAlphaOverride(std::uint32_t expressId) const;

    /// Per-type alpha override, keyed by ifcType. Value is clamped to [0,1].
    void SetTypeAlphaOverride(const std::string& ifcType, float alpha);
    void ClearTypeAlphaOverride(const std::string& ifcType);
    [[nodiscard]] std::optional<float> GetTypeAlphaOverride(const std::string& ifcType) const;

    /// Effective override for @p id: element override > type override > none.
    /// Returns nullopt if neither override is set for this node.
    [[nodiscard]] std::optional<float> GetEffectiveAlpha(NodeId id) const;

private:
    std::vector<SceneNode> nodes_;
    NodeId nextId_ = 0;
    std::unordered_map<std::uint32_t, float> elementAlphaOverrides_;
    std::unordered_map<std::string, float> typeAlphaOverrides_;
};

/// IFC types that represent non-visual concepts (spatial volumes, openings, grids)
/// and should be hidden by default on model load.
const std::vector<std::string>& DefaultHiddenTypes();

/// IFC types that should render with a baseline alpha override on load — e.g.
/// IfcWindow as semi-transparent glass. Applied via `SetTypeAlphaOverride`
/// after scene construction.
const std::vector<std::pair<std::string, float>>& DefaultTypeAlphaOverrides();

/// Apply a uniform "ghost" alpha to every type present in the scene except
/// IfcSlab and any type already carrying a default override (see
/// DefaultTypeAlphaOverrides). Used by the Point-of-View toolbar toggle so
/// floors stay solid and windows keep their glass alpha.
void ApplyPointOfViewAlpha(Scene& scene, float alpha);

/// Inverse of ApplyPointOfViewAlpha: clears the type alpha overrides that
/// Apply would have set, leaving IfcSlab and the default-override types
/// (e.g. IfcWindow at 0.4) untouched.
void ClearPointOfViewAlpha(Scene& scene);

} // namespace bimeup::scene
