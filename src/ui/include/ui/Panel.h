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

    [[nodiscard]] bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }
    void Open() { m_visible = true; }
    void Close() { m_visible = false; }
    void Toggle() { m_visible = !m_visible; }

private:
    bool m_visible = true;
};

}  // namespace bimeup::ui
