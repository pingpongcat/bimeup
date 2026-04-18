#include <renderer/Tonemap.h>

#include <glm/common.hpp>

namespace bimeup::renderer {

glm::vec3 AcesTonemap(const glm::vec3& hdrLinear) {
    constexpr float a = 2.51F;
    constexpr float b = 0.03F;
    constexpr float c = 2.43F;
    constexpr float d = 0.59F;
    constexpr float e = 0.14F;
    // Clamp negatives first: the rational curve is non-monotonic for x<0.
    glm::vec3 x = glm::max(hdrLinear, glm::vec3(0.0F));
    glm::vec3 num = x * ((a * x) + b);
    glm::vec3 den = (x * ((c * x) + d)) + e;
    return glm::clamp(num / den, glm::vec3(0.0F), glm::vec3(1.0F));
}

}  // namespace bimeup::renderer
