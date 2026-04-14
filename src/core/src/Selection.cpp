#include "core/Selection.h"

#include "core/EventBus.h"
#include "core/Events.h"

namespace bimeup::core {

Selection::Selection(EventBus& bus) : m_bus(bus) {
    m_subscription = m_bus.Subscribe<ElementSelected>(
        [this](const ElementSelected& event) { OnElementSelected(event); });
    m_clearSubscription = m_bus.Subscribe<SelectionCleared>(
        [this](const SelectionCleared&) { Clear(); });
}

Selection::~Selection() {
    m_bus.Unsubscribe<ElementSelected>(m_subscription);
    m_bus.Unsubscribe<SelectionCleared>(m_clearSubscription);
}

void Selection::Clear() {
    if (m_selected.empty()) {
        return;
    }
    m_selected.clear();
    if (m_onChanged) {
        m_onChanged();
    }
}

bool Selection::Contains(Id id) const {
    return m_selected.contains(id);
}

std::size_t Selection::Count() const {
    return m_selected.size();
}

const std::unordered_set<Selection::Id>& Selection::Ids() const {
    return m_selected;
}

void Selection::SetOnChanged(ChangedCallback callback) {
    m_onChanged = std::move(callback);
}

void Selection::OnElementSelected(const ElementSelected& event) {
    if (event.additive) {
        auto [it, inserted] = m_selected.insert(event.expressId);
        if (!inserted) {
            m_selected.erase(it);
        }
    } else {
        m_selected.clear();
        m_selected.insert(event.expressId);
    }
    if (m_onChanged) {
        m_onChanged();
    }
}

}  // namespace bimeup::core
