#include <renderer/BloomMath.h>

#include <algorithm>

namespace bimeup::renderer {

namespace {
constexpr float kLumaEps = 1e-4F;
}  // namespace

glm::vec3 BloomPrefilter(const glm::vec3& color, float threshold, float knee) {
    float luma = std::max(color.r, std::max(color.g, color.b));
    float softness = std::clamp(luma - threshold + knee, 0.0F, 2.0F * knee);
    softness = (softness * softness) / (4.0F * knee + kLumaEps);
    float contribution = std::max(softness, luma - threshold) /
                         std::max(luma, kLumaEps);
    return color * contribution;
}

glm::vec3 BloomDownsample(const glm::vec3& center,
                          const glm::vec3& topLeft, const glm::vec3& topRight,
                          const glm::vec3& bottomLeft, const glm::vec3& bottomRight) {
    return (4.0F * center + topLeft + topRight + bottomLeft + bottomRight) / 8.0F;
}

glm::vec3 BloomUpsample(const glm::vec3& top, const glm::vec3& bottom,
                        const glm::vec3& left, const glm::vec3& right,
                        const glm::vec3& topLeft, const glm::vec3& topRight,
                        const glm::vec3& bottomLeft, const glm::vec3& bottomRight) {
    return (top + bottom + left + right +
            2.0F * (topLeft + topRight + bottomLeft + bottomRight)) / 12.0F;
}

}  // namespace bimeup::renderer
