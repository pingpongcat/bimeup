#include <renderer/OctNormal.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>

namespace bimeup::renderer {

namespace {

// Fold the negative-Z hemisphere of the octahedron onto [-1, 1]^2. Matches the
// reference GLSL: `return (1 - abs(v.yx)) * (v.xy >= 0 ? 1 : -1)`.
glm::vec2 OctWrap(const glm::vec2& v) {
    glm::vec2 folded{1.0F - std::abs(v.y), 1.0F - std::abs(v.x)};
    glm::vec2 signV{v.x >= 0.0F ? 1.0F : -1.0F, v.y >= 0.0F ? 1.0F : -1.0F};
    return folded * signV;
}

}  // namespace

glm::vec2 OctPackNormal(const glm::vec3& n) {
    float l1 = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
    glm::vec3 nn = n / l1;
    glm::vec2 e{nn.x, nn.y};
    if (nn.z < 0.0F) {
        e = OctWrap(e);
    }
    return e;
}

glm::vec3 OctUnpackNormal(const glm::vec2& e) {
    glm::vec3 n{e.x, e.y, 1.0F - std::abs(e.x) - std::abs(e.y)};
    float t = glm::clamp(-n.z, 0.0F, 1.0F);
    n.x += n.x >= 0.0F ? -t : t;
    n.y += n.y >= 0.0F ? -t : t;
    return glm::normalize(n);
}

}  // namespace bimeup::renderer
