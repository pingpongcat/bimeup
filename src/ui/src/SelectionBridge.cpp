#include <ui/SelectionBridge.h>

#include <core/EventBus.h>
#include <core/Events.h>
#include <ui/PropertyPanel.h>

#include <utility>

namespace bimeup::ui {

SelectionBridge::SelectionBridge(core::EventBus& bus,
                                 PropertyPanel& propertyPanel,
                                 ElementLookup lookup)
    : m_bus(bus),
      m_propertyPanel(propertyPanel),
      m_lookup(std::move(lookup)) {
    m_subscription = m_bus.Subscribe<core::ElementSelected>(
        [this](const core::ElementSelected& event) { OnElementSelected(event); });
    m_clearSubscription = m_bus.Subscribe<core::SelectionCleared>(
        [this](const core::SelectionCleared&) { OnSelectionCleared(); });
}

SelectionBridge::~SelectionBridge() {
    m_bus.Unsubscribe<core::ElementSelected>(m_subscription);
    m_bus.Unsubscribe<core::SelectionCleared>(m_clearSubscription);
}

void SelectionBridge::Select(uint32_t expressId, bool additive) {
    m_bus.Publish(core::ElementSelected{expressId, additive});
}

void SelectionBridge::SetLookup(ElementLookup lookup) {
    m_lookup = std::move(lookup);
}

void SelectionBridge::OnSelectionCleared() {
    m_hasCurrent = false;
    m_propertyPanel.SetElement(nullptr);
}

void SelectionBridge::OnElementSelected(const core::ElementSelected& event) {
    std::optional<ifc::IfcElement> found;
    if (m_lookup) {
        found = m_lookup(event.expressId);
    }
    if (found.has_value()) {
        m_current = std::move(*found);
        m_hasCurrent = true;
        m_propertyPanel.SetElement(&m_current);
    } else {
        m_hasCurrent = false;
        m_propertyPanel.SetElement(nullptr);
    }
}

}  // namespace bimeup::ui
