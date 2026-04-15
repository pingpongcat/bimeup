#include <scene/Scene.h>

#include <algorithm>
#include <set>

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

std::vector<NodeId> Scene::FindByType(const std::string& ifcType) const {
    std::vector<NodeId> result;
    for (const auto& node : nodes_) {
        if (node.ifcType == ifcType) {
            result.push_back(node.id);
        }
    }
    return result;
}

} // namespace bimeup::scene
