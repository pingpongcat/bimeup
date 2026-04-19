#include <renderer/SsilMath.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

glm::mat4 ComputeReprojectionMatrix(const glm::mat4& prevViewProj,
                                    const glm::mat4& currInvViewProj) {
    return prevViewProj * currInvViewProj;
}

float SsilNormalRejectionWeight(const glm::vec3& currentNormal,
                                const glm::vec3& sampledNormal,
                                float strength) {
    float cosTheta = std::max(0.0F, glm::dot(currentNormal, sampledNormal));
    return std::pow(cosTheta, strength);
}

}  // namespace bimeup::renderer
