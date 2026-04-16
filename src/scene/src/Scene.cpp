#include <scene/Scene.h>

#include <algorithm>
#include <set>

namespace {
float ClampUnit(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}
}  // namespace

namespace bimeup::scene {

NodeId Scene::AddNode(SceneNode node) {
    NodeId id = nextId_++;
    node.id = id;

    // Register as child of parent if parent is valid
    if (node.parent != InvalidNodeId) {
        nodes_.at(node.parent).children.push_back(id);
    }

    nodes_.push_back(std::move(node));
    return id;
}

SceneNode& Scene::GetNode(NodeId id) {
    if (id >= nodes_.size()) {
        throw std::out_of_range("Invalid NodeId: " + std::to_string(id));
    }
    return nodes_[id];
}

const SceneNode& Scene::GetNode(NodeId id) const {
    if (id >= nodes_.size()) {
        throw std::out_of_range("Invalid NodeId: " + std::to_string(id));
    }
    return nodes_[id];
}

std::vector<NodeId> Scene::GetRoots() const {
    std::vector<NodeId> roots;
    for (const auto& node : nodes_) {
        if (node.parent == InvalidNodeId) {
            roots.push_back(node.id);
        }
    }
    return roots;
}

std::vector<NodeId> Scene::GetChildren(NodeId id) const {
    return GetNode(id).children;
}

size_t Scene::GetNodeCount() const {
    return nodes_.size();
}

void Scene::SetVisibility(NodeId id, bool visible, bool recursive) {
    auto& node = GetNode(id);
    node.visible = visible;
    if (recursive) {
        for (NodeId childId : node.children) {
            SetVisibility(childId, visible, true);
        }
    }
}

void Scene::SetSelected(NodeId id, bool selected) {
    GetNode(id).selected = selected;
}

std::vector<NodeId> Scene::GetSelected() const {
    std::vector<NodeId> selected;
    for (const auto& node : nodes_) {
        if (node.selected) {
            selected.push_back(node.id);
        }
    }
    return selected;
}

size_t Scene::IsolateByExpressId(const std::unordered_set<std::uint32_t>& expressIds) {
    size_t changed = 0;
    for (auto& node : nodes_) {
        if (!node.mesh.has_value()) {
            continue;
        }
        const bool keep = expressIds.count(node.expressId) > 0;
        if (node.visible != keep) {
            node.visible = keep;
            ++changed;
        }
    }
    return changed;
}

size_t Scene::IsolateByExpressId(std::initializer_list<std::uint32_t> expressIds) {
    return IsolateByExpressId(std::unordered_set<std::uint32_t>(expressIds));
}

void Scene::ShowAll() {
    for (auto& node : nodes_) {
        node.visible = true;
    }
}

size_t Scene::SetVisibilityByType(const std::string& ifcType, bool visible) {
    size_t affected = 0;
    for (auto& node : nodes_) {
        if (node.ifcType == ifcType) {
            node.visible = visible;
            ++affected;
        }
    }
    return affected;
}

std::vector<std::string> Scene::GetUniqueTypes() const {
    std::set<std::string> unique;
    for (const auto& node : nodes_) {
        if (!node.ifcType.empty()) {
            unique.insert(node.ifcType);
        }
    }
    return {unique.begin(), unique.end()};
}

const std::vector<std::string>& DefaultHiddenTypes() {
    static const std::vector<std::string> types = {
        "IfcSpace",
        "IfcOpeningElement",
        "IfcGrid",
        "IfcAnnotation",
    };
    return types;
}

const std::vector<std::pair<std::string, float>>& DefaultTypeAlphaOverrides() {
    static const std::vector<std::pair<std::string, float>> overrides = {
        {"IfcWindow", 0.4F},
    };
    return overrides;
}

namespace {
std::unordered_set<std::string> PointOfViewExcludedTypes() {
    std::unordered_set<std::string> excluded{"IfcSlab"};
    for (const auto& [type, _] : DefaultTypeAlphaOverrides()) {
        excluded.insert(type);
    }
    return excluded;
}
}  // namespace

void ApplyPointOfViewAlpha(Scene& scene, float alpha) {
    const auto excluded = PointOfViewExcludedTypes();
    for (const auto& type : scene.GetUniqueTypes()) {
        if (excluded.count(type) > 0) {
            continue;
        }
        scene.SetTypeAlphaOverride(type, alpha);
    }
}

void ClearPointOfViewAlpha(Scene& scene) {
    const auto excluded = PointOfViewExcludedTypes();
    for (const auto& type : scene.GetUniqueTypes()) {
        if (excluded.count(type) > 0) {
            continue;
        }
        scene.ClearTypeAlphaOverride(type);
    }
}

std::vector<NodeId> Scene::FindByType(const std::string& ifcType) const {
    std::vector<NodeId> result;
    for (const auto& node : nodes_) {
        if (node.ifcType == ifcType) {
            result.push_back(node.id);
        }
    }
    return result;
}

std::vector<NodeId> Scene::FindByExpressId(std::uint32_t expressId) const {
    std::vector<NodeId> result;
    for (const auto& node : nodes_) {
        if (node.expressId == expressId) {
            result.push_back(node.id);
        }
    }
    return result;
}

void Scene::SetElementAlphaOverride(std::uint32_t expressId, float alpha) {
    elementAlphaOverrides_[expressId] = ClampUnit(alpha);
}

void Scene::ClearElementAlphaOverride(std::uint32_t expressId) {
    elementAlphaOverrides_.erase(expressId);
}

std::optional<float> Scene::GetElementAlphaOverride(std::uint32_t expressId) const {
    auto it = elementAlphaOverrides_.find(expressId);
    if (it == elementAlphaOverrides_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Scene::SetTypeAlphaOverride(const std::string& ifcType, float alpha) {
    typeAlphaOverrides_[ifcType] = ClampUnit(alpha);
}

void Scene::ClearTypeAlphaOverride(const std::string& ifcType) {
    typeAlphaOverrides_.erase(ifcType);
}

std::optional<float> Scene::GetTypeAlphaOverride(const std::string& ifcType) const {
    auto it = typeAlphaOverrides_.find(ifcType);
    if (it == typeAlphaOverrides_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<float> Scene::GetEffectiveAlpha(NodeId id) const {
    const auto& node = GetNode(id);
    if (auto elem = GetElementAlphaOverride(node.expressId); elem.has_value()) {
        return elem;
    }
    return GetTypeAlphaOverride(node.ifcType);
}

} // namespace bimeup::scene
