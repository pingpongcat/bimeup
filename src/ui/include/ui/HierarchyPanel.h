#pragma once

#include <ifc/IfcHierarchy.h>
#include <ui/Panel.h>

#include <cstddef>
#include <cstdint>

namespace bimeup::core {
class EventBus;
}  // namespace bimeup::core

namespace bimeup::ui {

class HierarchyPanel : public Panel {
public:
    HierarchyPanel() = default;
    explicit HierarchyPanel(const ifc::HierarchyNode* root);

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetRoot(const ifc::HierarchyNode* root);
    [[nodiscard]] const ifc::HierarchyNode* GetRoot() const;

    [[nodiscard]] std::size_t GetDepth() const;
    [[nodiscard]] std::size_t GetNodeCount() const;

    void SetEventBus(core::EventBus* bus);
    void Select(std::uint32_t expressId, bool additive = false);

private:
    const ifc::HierarchyNode* m_root = nullptr;
    core::EventBus* m_bus = nullptr;

    void DrawNode(const ifc::HierarchyNode& node);
};

}  // namespace bimeup::ui
