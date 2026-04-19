#include "renderer/SmaaMath.h"

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

float SmaaLuminance(glm::vec3 rgb) {
    return (rgb.r * 0.2126F) + (rgb.g * 0.7152F) + (rgb.b * 0.0722F);
}

glm::bvec2 SmaaDetectEdgeLuma(
    float L,
    float Lleft,
    float Ltop,
    float Lright,
    float Lbottom,
    float threshold,
    float localContrastFactor) {
    const float deltaL = std::abs(L - Lleft);
    const float deltaT = std::abs(L - Ltop);
    const float deltaR = std::abs(L - Lright);
    const float deltaB = std::abs(L - Lbottom);

    bool edgeX = deltaL > threshold;
    bool edgeY = deltaT > threshold;

    const float maxNeighbour = std::max({deltaL, deltaT, deltaR, deltaB});
    if (deltaL * localContrastFactor < maxNeighbour) {
        edgeX = false;
    }
    if (deltaT * localContrastFactor < maxNeighbour) {
        edgeY = false;
    }

    return {edgeX, edgeY};
}

}  // namespace bimeup::renderer
