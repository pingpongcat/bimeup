#include <renderer/ViewportNavigator.h>

namespace bimeup::renderer {

NavAction ClassifyDrag(NavButton button, NavModifiers mods) {
    if (button != NavButton::Middle) return NavAction::None;
    if (mods.shift) return NavAction::Pan;
    if (mods.ctrl) return NavAction::Dolly;
    return NavAction::Orbit;
}

}  // namespace bimeup::renderer
