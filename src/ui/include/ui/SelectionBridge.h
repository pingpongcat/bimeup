#pragma once

#include <ifc/IfcElement.h>

#include <cstdint>
#include <functional>
#include <optional>

namespace bimeup::core {
class EventBus;
struct ElementSelected;
}  // namespace bimeup::core

namespace bimeup::ui {

class PropertyPanel;

class SelectionBridge {
public:
    using ElementLookup =
        std::function<std::optional<ifc::IfcElement>(uint32_t expressId)>;

    SelectionBridge(core::EventBus& bus,
                    PropertyPanel& propertyPanel,
                    ElementLookup lookup);
    ~SelectionBridge();

    SelectionBridge(const SelectionBridge&) = delete;
    SelectionBridge& operator=(const SelectionBridge&) = delete;
    SelectionBridge(SelectionBridge&&) = delete;
    SelectionBridge& operator=(SelectionBridge&&) = delete;

    void Select(uint32_t expressId, bool additive = false);

    void SetLookup(ElementLookup lookup);

private:
    void OnElementSelected(const core::ElementSelected& event);

    core::EventBus& m_bus;
    PropertyPanel& m_propertyPanel;
    ElementLookup m_lookup;
    uint32_t m_subscription = 0;
    ifc::IfcElement m_current;
    bool m_hasCurrent = false;
};

}  // namespace bimeup::ui
