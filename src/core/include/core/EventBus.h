#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace bimeup::core {

class EventBus {
public:
    template <typename T>
    using Handler = std::function<void(const T&)>;

    template <typename T>
    uint32_t Subscribe(Handler<T> handler) {
        auto& list = m_handlers[std::type_index(typeid(T))];
        uint32_t id = ++m_nextId;
        list.push_back({id, [cb = std::move(handler)](const void* event) {
                            cb(*static_cast<const T*>(event));
                        }});
        return id;
    }

    template <typename T>
    void Unsubscribe(uint32_t id) {
        auto it = m_handlers.find(std::type_index(typeid(T)));
        if (it == m_handlers.end()) {
            return;
        }
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [id](const Entry& e) { return e.id == id; }),
                   list.end());
    }

    template <typename T>
    void Publish(const T& event) {
        auto it = m_handlers.find(std::type_index(typeid(T)));
        if (it == m_handlers.end()) {
            return;
        }
        for (const auto& entry : it->second) {
            entry.callback(&event);
        }
    }

private:
    struct Entry {
        uint32_t id;
        std::function<void(const void*)> callback;
    };

    std::unordered_map<std::type_index, std::vector<Entry>> m_handlers;
    uint32_t m_nextId = 0;
};

}  // namespace bimeup::core
