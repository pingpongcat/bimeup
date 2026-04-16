#pragma once

#include <ifc/IfcElement.h>
#include <ui/Panel.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bimeup::ui {

class PropertyPanel : public Panel {
public:
    using Property = std::pair<std::string, std::string>;

    PropertyPanel() = default;
    explicit PropertyPanel(const ifc::IfcElement* element);

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetElement(const ifc::IfcElement* element);
    [[nodiscard]] const ifc::IfcElement* GetElement() const;
    [[nodiscard]] bool HasElement() const;

    [[nodiscard]] std::size_t GetPropertyCount() const;
    [[nodiscard]] std::string_view GetPropertyKey(std::size_t index) const;
    [[nodiscard]] std::string_view GetPropertyValue(std::size_t index) const;

private:
    const ifc::IfcElement* m_element = nullptr;
    std::vector<Property> m_properties;

    void Rebuild();
};

}  // namespace bimeup::ui
