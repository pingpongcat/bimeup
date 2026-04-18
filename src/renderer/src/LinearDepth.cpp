#include <renderer/LinearDepth.h>

namespace bimeup::renderer {

float LinearizeDepth(float nonLinearDepth, float nearZ, float farZ) {
    // Equivalent to `(near * far) / (far - z_nl * (far - near))` but written as
    // a lerp of far/near in the denominator — avoids catastrophic cancellation
    // at z_nl ≈ 1 (old form subtracts two near-equal floats, drifting ~1e-3
    // away from `far` at single precision).
    return (nearZ * farZ) / (farZ * (1.0F - nonLinearDepth) + nearZ * nonLinearDepth);
}

glm::vec3 ReconstructViewPosFromDepth(const glm::vec2& uv,
                                      float linearDepth,
                                      const glm::mat4& invProj) {
    // Shoot a ray through the uv at the far NDC plane (z_nl = 1 works for any
    // non-degenerate projection); any finite z_nl would do.
    glm::vec4 ndc{uv.x * 2.0F - 1.0F, uv.y * 2.0F - 1.0F, 1.0F, 1.0F};
    glm::vec4 viewH = invProj * ndc;
    glm::vec3 view{viewH.x / viewH.w, viewH.y / viewH.w, viewH.z / viewH.w};
    // Rescale so view.z == -linearDepth. view.z is < 0 for any valid point in
    // front of the camera, so dividing yields a positive scale factor.
    float scale = -linearDepth / view.z;
    return view * scale;
}

}  // namespace bimeup::renderer
