#pragma once

namespace bimeup::renderer {

enum class NavButton { Left, Middle, Right };

enum class NavAction { None, Orbit, Pan, Dolly };

struct NavModifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

NavAction ClassifyDrag(NavButton button, NavModifiers mods);

}  // namespace bimeup::renderer
