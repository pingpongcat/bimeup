#include <ui/HierarchyPanel.h>

#include <imgui.h>

#include <string>

namespace bimeup::ui {

namespace {

std::size_t ComputeDepth(const ifc::HierarchyNode& node) {
    std::size_t maxChild = 0;
    for (const auto& child : node.children) {
        maxChild = std::max(maxChild, ComputeDepth(child));
    }
    return 1 + maxChild;
}

std::size_t ComputeNodeCount(const ifc::HierarchyNode& node) {
    std::size_t count = 1;
    for (const auto& child : node.children) {
        count += ComputeNodeCount(child);
    }
    return count;
}

std::string FormatLabel(const ifc::HierarchyNode& node) {
    std::string label = node.type;
    if (!node.name.empty()) {
        label += " \"";
        label += node.name;
        label += "\"";
    }
    label += "##";
    label += std::to_string(node.expressId);
    return label;
}

}  // namespace

HierarchyPanel::HierarchyPanel(const ifc::HierarchyNode* root) : m_root(root) {}

const char* HierarchyPanel::GetName() const {
    return "Hierarchy";
}

void HierarchyPanel::SetRoot(const ifc::HierarchyNode* root) {
    m_root = root;
}

const ifc::HierarchyNode* HierarchyPanel::GetRoot() const {
    return m_root;
}

std::size_t HierarchyPanel::GetDepth() const {
    if (m_root == nullptr) {
        return 0;
    }
    return ComputeDepth(*m_root);
}

std::size_t HierarchyPanel::GetNodeCount() const {
    if (m_root == nullptr) {
        return 0;
    }
    return ComputeNodeCount(*m_root);
}

void HierarchyPanel::OnDraw() {
    if (ImGui::Begin(GetName())) {
        if (m_root != nullptr) {
            DrawNode(*m_root);
        } else {
            ImGui::TextUnformatted("No model loaded");
        }
    }
    ImGui::End();
}

void HierarchyPanel::DrawNode(const ifc::HierarchyNode& node) {
    const std::string label = FormatLabel(node);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(label.c_str(), flags);
        return;
    }
    if (ImGui::TreeNodeEx(label.c_str(), flags)) {
        for (const auto& child : node.children) {
            DrawNode(child);
        }
        ImGui::TreePop();
    }
}

}  // namespace bimeup::ui
