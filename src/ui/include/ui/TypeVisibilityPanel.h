#pragma once

#include <ui/Panel.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace bimeup::scene {
class Scene;
}

namespace bimeup::ui {

/// Lists every unique IfcType present in the bound scene and lets the user
/// toggle visibility for all elements of that type. Types in
/// `scene::DefaultHiddenTypes()` are hidden on first bind via `ApplyDefaults`.
class TypeVisibilityPanel : public Panel {
public:
    TypeVisibilityPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetScene(scene::Scene* scene);
    [[nodiscard]] scene::Scene* GetScene() const { return m_scene; }

    /// Rebuild the cached type list + visibility flags from the bound scene.
    void Refresh();

    /// Hide every type in `scene::DefaultHiddenTypes()` that exists in the scene.
    /// Call once after binding a freshly-loaded scene.
    void ApplyDefaults();

    [[nodiscard]] const std::vector<std::string>& GetTypes() const { return m_types; }
    [[nodiscard]] bool IsTypeVisible(const std::string& ifcType) const;
    void SetTypeVisible(const std::string& ifcType, bool visible);

private:
    scene::Scene* m_scene = nullptr;
    std::vector<std::string> m_types;
    std::unordered_map<std::string, bool> m_visible;
};

}  // namespace bimeup::ui
