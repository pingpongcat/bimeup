#pragma once

#include <glm/glm.hpp>

#include <array>

namespace bimeup::renderer {

struct SkyColor {
    // 3-tone hemisphere ambient triple (matches `HemisphereAmbient` in
    // Lighting.h: zenith = +Y, ground = -Y, horizon = lateral).
    glm::vec3 zenith;
    glm::vec3 horizon;
    glm::vec3 ground;
    // Directional-sun colour tint. Warm at low elevation, white at high.
    glm::vec3 sunColor;
};

// 5-key sun-elevation LUT, ascending. Order: night, civil-twilight, golden,
// day, zenith-sun. Values outside the outer keys clamp.
inline constexpr int kSkyColorKeyCount = 5;
inline constexpr std::array<float, kSkyColorKeyCount> kSkyColorKeyElevations =
    {{
        -0.17453292F,  // -10° : night
        -0.05235988F,  //  -3° : civil twilight
         0.08726646F,  //  +5° : golden hour
         0.52359878F,  // +30° : day
         1.04719755F,  // +60° : zenith sun
    }};

// Piecewise-linear LUT lookup. C⁰-continuous at key boundaries (sampling at
// `kSkyColorKeyElevations[i]` returns key i exactly).
SkyColor ComputeSkyColor(float sunElevationRad);

// Returns the i-th LUT key as-is. Test/panel-preview accessor.
// Index is clamped to `[0, kSkyColorKeyCount)`.
SkyColor SkyColorKeyAt(int index);

}  // namespace bimeup::renderer
