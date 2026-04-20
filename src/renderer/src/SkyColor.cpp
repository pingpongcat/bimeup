#include <renderer/SkyColor.h>

#include <algorithm>
#include <array>

namespace bimeup::renderer {

namespace {

// LUT keys aligned 1:1 with `kSkyColorKeyElevations`. Tuned by eye for an
// architectural viewer: each key is a recognisable lighting moment, and the
// in-between lerps land plausibly. Keep this table as the single source of
// truth — the panel preview reads it via `SkyColorKeyAt`.
constexpr std::array<SkyColor, kSkyColorKeyCount> kKeys = {{
    // night: very dim, cool, no direct sun
    {{0.010F, 0.012F, 0.030F},
     {0.020F, 0.022F, 0.045F},
     {0.015F, 0.015F, 0.020F},
     {0.000F, 0.000F, 0.000F}},
    // civil twilight: indigo zenith, dusky-warm horizon, deep-red sun
    {{0.100F, 0.110F, 0.220F},
     {0.350F, 0.250F, 0.300F},
     {0.080F, 0.070F, 0.080F},
     {0.500F, 0.220F, 0.120F}},
    // golden hour: warm horizon and orange sun
    {{0.400F, 0.520F, 0.780F},
     {1.000F, 0.650F, 0.400F},
     {0.450F, 0.400F, 0.320F},
     {1.000F, 0.620F, 0.350F}},
    // day: standard blue sky, slightly warm sun
    {{0.520F, 0.620F, 0.840F},
     {0.780F, 0.830F, 0.940F},
     {0.450F, 0.420F, 0.380F},
     {1.000F, 0.950F, 0.880F}},
    // zenith sun: bright blue sky, neutral-white sun
    {{0.480F, 0.620F, 0.900F},
     {0.740F, 0.820F, 0.920F},
     {0.500F, 0.460F, 0.420F},
     {1.000F, 1.000F, 1.000F}},
}};

glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + ((b - a) * t);
}

SkyColor LerpSky(const SkyColor& a, const SkyColor& b, float t) {
    return {
        Lerp(a.zenith, b.zenith, t),
        Lerp(a.horizon, b.horizon, t),
        Lerp(a.ground, b.ground, t),
        Lerp(a.sunColor, b.sunColor, t),
    };
}

}  // namespace

SkyColor ComputeSkyColor(float sunElevationRad) {
    if (sunElevationRad <= kSkyColorKeyElevations[0]) {
        return kKeys[0];
    }
    if (sunElevationRad >= kSkyColorKeyElevations[kSkyColorKeyCount - 1]) {
        return kKeys[kSkyColorKeyCount - 1];
    }
    // Find the bracketing pair: kKeys[i] ≤ sunElevation < kKeys[i+1].
    for (int i = 0; i < kSkyColorKeyCount - 1; ++i) {
        const float lo = kSkyColorKeyElevations[i];
        const float hi = kSkyColorKeyElevations[i + 1];
        if (sunElevationRad < hi) {
            const float t = (sunElevationRad - lo) / (hi - lo);
            return LerpSky(kKeys[i], kKeys[i + 1], t);
        }
    }
    // Unreachable: the upper-clamp branch above covers `>= last key`.
    return kKeys[kSkyColorKeyCount - 1];
}

SkyColor SkyColorKeyAt(int index) {
    const int clamped = std::clamp(index, 0, kSkyColorKeyCount - 1);
    return kKeys[clamped];
}

}  // namespace bimeup::renderer
