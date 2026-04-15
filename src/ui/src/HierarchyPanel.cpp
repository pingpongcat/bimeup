#include <ui/HierarchyPanel.h>

#include <core/EventBus.h>
#include <core/Events.h>
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

HierarchyPanel::~HierarchyPanel() {
    UnsubscribeFromBus();
}

void HierarchyPanel::SetEventBus(core::EventBus* bus) {
    if (m_bus == bus) {
        return;
    }
    UnsubscribeFromBus();
    m_bus = bus;
    if (m_bus == nullptr) {
        return;
    }
    m_subSelected = m_bus->Subscribe<core::ElementSelected>(
        [this](const core::ElementSelected& e) { HandleSelected(e.expressId, e.additive); });
    m_subCleared = m_bus->Subscribe<core::SelectionCleared>(
        [this](const core::SelectionCleared&) { HandleCleared(); });
}

void HierarchyPanel::UnsubscribeFromBus() {
    if (m_bus == nullptr) {
        return;
    }
    if (m_subSelected != 0) {
        m_bus->Unsubscribe<core::ElementSelected>(m_subSelected);
        m_subSelected = 0;
    }
    if (m_subCleared != 0) {
        m_bus->Unsubscribe<core::SelectionCleared>(m_subCleared);
        m_subCleared = 0;
    }
}

void HierarchyPanel::HandleSelected(std::uint32_t expressId, bool additive) {
    if (!additive) {
        m_selectedIds.clear();
    }
    m_selectedIds.insert(expressId);
    RecomputeAncestors();
}

void HierarchyPanel::HandleCleared() {
    m_selectedIds.clear();
    m_ancestorIds.clear();
}

void HierarchyPanel::RecomputeAncestors() {
    m_ancestorIds.clear();
    if (m_root == nullptr) {
        return;
    }
    for (std::uint32_t id : m_selectedIds) {
        CollectAncestorPath(*m_root, id, m_ancestorIds);
    }
}

bool HierarchyPanel::CollectAncestorPath(const ifc::HierarchyNode& node,
                                         std::uint32_t targetId,
                                         std::unordered_set<std::uint32_t>& out) const {
    if (node.expressId == targetId) {
        return true;
    }
    for (const auto& child : node.children) {
        if (CollectAncestorPath(child, targetId, out)) {
            out.insert(node.expressId);
            return true;
        }
    }
    return false;
}

bool HierarchyPanel::IsSelected(std::uint32_t expressId) const {
    return m_selectedIds.count(expressId) > 0;
}

bool HierarchyPanel::IsAncestorOfSelection(std::uint32_t expressId) const {
    return m_ancestorIds.count(expressId) > 0;
}

void HierarchyPanel::SetOnToggleVisibility(NodeCallback cb) {
    m_onToggleVisibility = std::move(cb);
}

void HierarchyPanel::SetOnIsolate(NodeCallback cb) {
    m_onIsolate = std::move(cb);
}

void HierarchyPanel::SetVisibilityQuery(VisibilityQuery q) {
    m_visibilityQuery = std::move(q);
}

void HierarchyPanel::TriggerToggleVisibility(const ifc::HierarchyNode& node) {
    if (m_onToggleVisibility) {
        m_onToggleVisibility(node);
    }
}

void HierarchyPanel::TriggerIsolate(const ifc::HierarchyNode& node) {
    if (m_onIsolate) {
        m_onIsolate(node);
    }
}

void HierarchyPanel::Select(std::uint32_t expressId, bool additive) {
    if (m_bus != nullptr) {
        m_bus->Publish(core::ElementSelected{expressId, additive});
    }
}

const char* HierarchyPanel::GetName() const {
    return "Hierarchy";
}

void HierarchyPanel::SetRoot(const ifc::HierarchyNode* root) {
    m_root = root;
    RecomputeAncestors();
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
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_AllowOverlap;
    if (IsSelected(node.expressId)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (IsAncestorOfSelection(node.expressId)) {
        ImGui::SetNextItemOpen(true);
    }
    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(label.c_str(), flags);
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            Select(node.expressId, ImGui::GetIO().KeyCtrl);
        }
        DrawRowIcons(node);
        return;
    }
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        Select(node.expressId, ImGui::GetIO().KeyCtrl);
    }
    DrawRowIcons(node);
    if (open) {
        for (const auto& child : node.children) {
            DrawNode(child);
        }
        ImGui::TreePop();
    }
}

void HierarchyPanel::DrawRowIcons(const ifc::HierarchyNode& node) {
    ImGui::PushID(static_cast<int>(node.expressId));
    constexpr float kIconCol = 44.0F;  // reserved width for "eye + I"
    const float rightEdge = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(rightEdge - kIconCol);

    const bool visible = m_visibilityQuery ? m_visibilityQuery(node) : true;
    const char* eyeLabel = visible ? "o" : "-";
    if (ImGui::SmallButton(eyeLabel)) {
        TriggerToggleVisibility(node);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(visible ? "Hide" : "Show");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("I")) {
        TriggerIsolate(node);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Isolate");
    }
    ImGui::PopID();
}

}  // namespace bimeup::ui
