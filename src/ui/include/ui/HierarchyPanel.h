#pragma once

#include <ifc/IfcHierarchy.h>
#include <ui/Panel.h>

#include <cstddef>

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

private:
    const ifc::HierarchyNode* m_root = nullptr;

    void DrawNode(const ifc::HierarchyNode& node);
};

}  // namespace bimeup::ui
