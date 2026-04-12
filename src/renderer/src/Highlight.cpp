#include <renderer/Highlight.h>

namespace bimeup::renderer {

Highlight::Highlight(glm::vec4 color) : m_color(color) {}

void Highlight::Select(ElementId id) {
    m_selected.insert(id);
}

void Highlight::Deselect(ElementId id) {
    m_selected.erase(id);
}

void Highlight::Clear() {
    m_selected.clear();
}

void Highlight::SetColor(glm::vec4 color) {
    m_color = color;
}

glm::vec4 Highlight::GetColor() const {
    return m_color;
}

bool Highlight::IsSelected(ElementId id) const {
    return m_selected.find(id) != m_selected.end();
}

std::optional<glm::vec4> Highlight::GetOverrideColor(ElementId id) const {
    if (IsSelected(id)) {
        return m_color;
    }
    return std::nullopt;
}

std::size_t Highlight::Count() const {
    return m_selected.size();
}

const std::unordered_set<ElementId>& Highlight::Ids() const {
    return m_selected;
}

}  // namespace bimeup::renderer
