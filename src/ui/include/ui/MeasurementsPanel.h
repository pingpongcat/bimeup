#pragma once

#include <ui/Panel.h>

#include <cstddef>
#include <functional>

namespace bimeup::scene {
class MeasureTool;
}

namespace bimeup::ui {

class MeasurementsPanel : public Panel {
public:
    using ClearAllCallback = std::function<void()>;

    MeasurementsPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetTool(scene::MeasureTool* tool) { m_tool = tool; }
    void SetOnClearAll(ClearAllCallback cb) { m_onClearAll = std::move(cb); }

private:
    scene::MeasureTool* m_tool = nullptr;
    ClearAllCallback m_onClearAll;
};

}  // namespace bimeup::ui
