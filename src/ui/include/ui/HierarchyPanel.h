#pragma once

#include <ifc/IfcHierarchy.h>
#include <ui/Panel.h>

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace bimeup::core {
class EventBus;
}  // namespace bimeup::core

namespace bimeup::ui {

class HierarchyPanel : public Panel {
public:
    HierarchyPanel() = default;
    explicit HierarchyPanel(const ifc::HierarchyNode* root);
    ~HierarchyPanel() override;

    HierarchyPanel(const HierarchyPanel&) = delete;
    HierarchyPanel& operator=(const HierarchyPanel&) = delete;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetRoot(const ifc::HierarchyNode* root);
    [[nodiscard]] const ifc::HierarchyNode* GetRoot() const;

    [[nodiscard]] std::size_t GetDepth() const;
    [[nodiscard]] std::size_t GetNodeCount() const;

    void SetEventBus(core::EventBus* bus);
    void Select(std::uint32_t expressId, bool additive = false);

    [[nodiscard]] bool IsSelected(std::uint32_t expressId) const;
    [[nodiscard]] bool IsAncestorOfSelection(std::uint32_t expressId) const;

private:
    const ifc::HierarchyNode* m_root = nullptr;
    core::EventBus* m_bus = nullptr;
    std::uint32_t m_subSelected = 0;
    std::uint32_t m_subCleared = 0;
    std::unordered_set<std::uint32_t> m_selectedIds;
    std::unordered_set<std::uint32_t> m_ancestorIds;

    void DrawNode(const ifc::HierarchyNode& node);
    void UnsubscribeFromBus();
    void HandleSelected(std::uint32_t expressId, bool additive);
    void HandleCleared();
    void RecomputeAncestors();
    bool CollectAncestorPath(const ifc::HierarchyNode& node,
                             std::uint32_t targetId,
                             std::unordered_set<std::uint32_t>& out) const;
};

}  // namespace bimeup::ui
