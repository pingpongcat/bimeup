#include <ui/PropertyPanel.h>

#include <imgui.h>

namespace bimeup::ui {

PropertyPanel::PropertyPanel(const ifc::IfcElement* element) {
    SetElement(element);
}

const char* PropertyPanel::GetName() const {
    return "Properties";
}

void PropertyPanel::SetElement(const ifc::IfcElement* element) {
    m_element = element;
    Rebuild();
    if (m_element != nullptr && m_alphaQuery) {
        m_alpha = m_alphaQuery(m_element->expressId);
    } else {
        m_alpha.reset();
    }
}

void PropertyPanel::SetOnAlphaChange(AlphaCallback cb) {
    m_onAlphaChange = std::move(cb);
}

void PropertyPanel::SetAlphaQuery(AlphaQuery q) {
    m_alphaQuery = std::move(q);
}

void PropertyPanel::TriggerAlphaChange(float alpha) {
    if (m_element == nullptr) {
        return;
    }
    m_alpha = alpha;
    if (m_onAlphaChange) {
        m_onAlphaChange(m_element->expressId, m_alpha);
    }
}

void PropertyPanel::TriggerClearAlpha() {
    if (m_element == nullptr) {
        return;
    }
    m_alpha.reset();
    if (m_onAlphaChange) {
        m_onAlphaChange(m_element->expressId, std::nullopt);
    }
}

const ifc::IfcElement* PropertyPanel::GetElement() const {
    return m_element;
}

bool PropertyPanel::HasElement() const {
    return m_element != nullptr;
}

std::size_t PropertyPanel::GetPropertyCount() const {
    return m_properties.size();
}

std::string_view PropertyPanel::GetPropertyKey(std::size_t index) const {
    return m_properties[index].first;
}

std::string_view PropertyPanel::GetPropertyValue(std::size_t index) const {
    return m_properties[index].second;
}

void PropertyPanel::Rebuild() {
    m_properties.clear();
    if (m_element == nullptr) {
        return;
    }
    m_properties.emplace_back("ExpressId", std::to_string(m_element->expressId));
    if (!m_element->type.empty()) {
        m_properties.emplace_back("Type", m_element->type);
    }
    if (!m_element->name.empty()) {
        m_properties.emplace_back("Name", m_element->name);
    }
    if (!m_element->globalId.empty()) {
        m_properties.emplace_back("GlobalId", m_element->globalId);
    }
}

void PropertyPanel::OnDraw() {
    if (ImGui::Begin(GetName())) {
        if (m_element == nullptr) {
            ImGui::TextUnformatted("No element selected");
        } else {
            if (ImGui::BeginTable("properties", 2,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Property");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (const auto& [key, value] : m_properties) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            ImGui::TextUnformatted("Alpha override");
            bool enabled = m_alpha.has_value();
            if (ImGui::Checkbox("##alphaEnabled", &enabled)) {
                if (enabled) {
                    TriggerAlphaChange(m_alpha.value_or(0.5f));
                } else {
                    TriggerClearAlpha();
                }
            }
            ImGui::SameLine();
            float value = m_alpha.value_or(1.0f);
            ImGui::BeginDisabled(!m_alpha.has_value());
            if (ImGui::SliderFloat("##alpha", &value, 0.0f, 1.0f, "%.2f")) {
                TriggerAlphaChange(value);
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
}

}  // namespace bimeup::ui
