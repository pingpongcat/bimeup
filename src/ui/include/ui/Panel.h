#pragma once

namespace bimeup::ui {

class Panel {
public:
    Panel() = default;
    virtual ~Panel() = default;

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;
    Panel(Panel&&) = delete;
    Panel& operator=(Panel&&) = delete;

    [[nodiscard]] virtual const char* GetName() const = 0;
    virtual void OnDraw() = 0;
};

}  // namespace bimeup::ui
