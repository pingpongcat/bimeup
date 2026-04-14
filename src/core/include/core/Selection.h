#pragma once

#include <cstdint>
#include <functional>
#include <unordered_set>

namespace bimeup::core {

class EventBus;
struct ElementSelected;

class Selection {
public:
    using Id = uint32_t;
    using ChangedCallback = std::function<void()>;

    explicit Selection(EventBus& bus);
    ~Selection();

    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;
    Selection(Selection&&) = delete;
    Selection& operator=(Selection&&) = delete;

    void Clear();

    [[nodiscard]] bool Contains(Id id) const;
    [[nodiscard]] std::size_t Count() const;
    [[nodiscard]] const std::unordered_set<Id>& Ids() const;

    void SetOnChanged(ChangedCallback callback);

private:
    void OnElementSelected(const ElementSelected& event);

    EventBus& m_bus;
    uint32_t m_subscription = 0;
    uint32_t m_clearSubscription = 0;
    std::unordered_set<Id> m_selected;
    ChangedCallback m_onChanged;
};

}  // namespace bimeup::core
