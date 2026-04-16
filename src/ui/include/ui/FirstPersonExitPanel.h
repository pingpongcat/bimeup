#pragma once

#include <ui/Panel.h>

#include <functional>

namespace bimeup::ui {

class FirstPersonExitPanel : public Panel {
public:
    using ExitCallback = std::function<void()>;

    FirstPersonExitPanel();

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnExit(ExitCallback callback);
    void TriggerExit();

private:
    ExitCallback m_onExit;
};

}  // namespace bimeup::ui
