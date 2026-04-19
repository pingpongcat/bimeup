#include <renderer/XeGtaoMath.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bimeup::renderer {

namespace {
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kHalfPi = 0.5F * kPi;
}  // namespace

glm::vec2 XeGtaoSliceDirection(std::uint32_t index, std::uint32_t sliceCount,
                               float jitter) {
    float phi = kPi * (static_cast<float>(index) + jitter) /
                static_cast<float>(sliceCount);
    return {std::cos(phi), std::sin(phi)};
}

float XeGtaoSliceVisibility(float horizonLeft, float horizonRight,
                            float normalAngle, float projectedNormalLen) {
    float h1 = std::max(horizonLeft, normalAngle - kHalfPi);
    float h2 = std::min(horizonRight, normalAngle + kHalfPi);
    float cosN = std::cos(normalAngle);
    float sinN = std::sin(normalAngle);
    float c1 = std::cos((2.0F * h1) - normalAngle);
    float c2 = std::cos((2.0F * h2) - normalAngle);
    float visArea = 0.25F * (-c1 + cosN + (2.0F * h1 * sinN) - c2 + cosN +
                             (2.0F * h2 * sinN));
    return projectedNormalLen * visArea;
}

}  // namespace bimeup::renderer
