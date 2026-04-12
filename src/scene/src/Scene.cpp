#include <scene/Scene.h>

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
