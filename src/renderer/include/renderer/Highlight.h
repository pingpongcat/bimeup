#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <unordered_set>

namespace bimeup::renderer {

using ElementId = uint32_t;

class Highlight {
public:
    static constexpr glm::vec4 DefaultColor{1.0f, 0.6f, 0.1f, 1.0f};

    Highlight() = default;
    explicit Highlight(glm::vec4 color);

    void Select(ElementId id);
    void Deselect(ElementId id);
    void Clear();

    void SetColor(glm::vec4 color);
    [[nodiscard]] glm::vec4 GetColor() const;

    [[nodiscard]] bool IsSelected(ElementId id) const;
    [[nodiscard]] std::optional<glm::vec4> GetOverrideColor(ElementId id) const;
    [[nodiscard]] std::size_t Count() const;
    [[nodiscard]] const std::unordered_set<ElementId>& Ids() const;

private:
    std::unordered_set<ElementId> m_selected;
    glm::vec4 m_color = DefaultColor;
};

}  // namespace bimeup::renderer
