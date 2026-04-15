#pragma once

#include <ifc/IfcElement.h>
#include <ui/Panel.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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

    // ----- 7.8d Alpha override ----------------------------------------
    using AlphaCallback =
        std::function<void(std::uint32_t expressId, std::optional<float> alpha)>;
    using AlphaQuery = std::function<std::optional<float>(std::uint32_t expressId)>;

    /// Host-owned callback. Fired on slider edit (with value) and on the
    /// clear button (with std::nullopt). No-op if no element is selected.
    void SetOnAlphaChange(AlphaCallback cb);
    /// Optional. Queried on `SetElement` to seed the slider from the scene's
    /// current override for that expressId.
    void SetAlphaQuery(AlphaQuery q);
    [[nodiscard]] std::optional<float> GetCurrentAlpha() const { return m_alpha; }

    // Test-friendly triggers (same code path as the UI widgets).
    void TriggerAlphaChange(float alpha);
    void TriggerClearAlpha();

private:
    const ifc::IfcElement* m_element = nullptr;
    std::vector<Property> m_properties;

    AlphaCallback m_onAlphaChange;
    AlphaQuery m_alphaQuery;
    std::optional<float> m_alpha;

    void Rebuild();
};

}  // namespace bimeup::ui
