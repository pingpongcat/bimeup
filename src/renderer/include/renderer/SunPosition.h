#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

struct SunPosition {
    // Unit vector pointing FROM the sun TOWARD the scene, in the renderer
    // world frame. Matches `DirectionalLight.direction` (Lighting.h), so it
    // can be assigned directly to the key-light slot.
    glm::vec3 dirWorld;

    // Radians above the horizon. Negative when the sun is below.
    float elevation;

    // Radians, compass bearing CW from model-frame +Z (north), in [0, 2π).
    // Includes `trueNorthRad`.
    float azimuth;
};

// NOAA-simplified solar position (~0.1° accuracy for 1900–2100; adequate for
// architectural-viewer shading). All angles are radians; longitude is positive
// east. `julianDay` is in UTC (UT1 ≈ UTC at this precision).
//
// World-frame convention used by the outputs:
//   +Y is up; the ground plane is XZ; +Z is model-frame north (so
//   `trueNorthRad = 0` means the model is already aligned to true north);
//   +X is east when `trueNorthRad = 0`. Azimuth is CW from +Z when viewed
//   from above (compass convention).
//
// `trueNorthRad` rotates the model frame relative to true north around +Y.
// It is added to the geographic azimuth to produce the model-frame azimuth
// returned in `azimuth`, and `dirWorld` is rotated by the same amount.
SunPosition ComputeSunDirection(double julianDay,
                                double latitudeRad,
                                double longitudeRad,
                                double trueNorthRad);

}  // namespace bimeup::renderer
