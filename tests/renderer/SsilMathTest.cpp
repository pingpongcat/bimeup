#include <gtest/gtest.h>

#include <renderer/SsilMath.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace {

using bimeup::renderer::ComputeReprojectionMatrix;
using bimeup::renderer::SsilNormalRejectionWeight;

constexpr float kEps = 1e-5F;

TEST(SsilMathTest, ReprojectionIdentityGivesIdentity) {
    // When the previous and current camera are identical (both = I), the
    // reprojection matrix must be identity. A non-identity result here would
    // silently offset the previous-frame HDR sample in ssil_main.comp by a
    // sub-pixel amount every frame, smearing the indirect lighting.
    glm::mat4 reproj = ComputeReprojectionMatrix(glm::mat4(1.0F),
                                                 glm::mat4(1.0F));
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float expected = (col == row) ? 1.0F : 0.0F;
            EXPECT_NEAR(reproj[col][row], expected, kEps)
                << "reproj[" << col << "][" << row << "]";
        }
    }
}

TEST(SsilMathTest, ReprojectionSelfRoundTripReturnsSameClip) {
    // Prev == Curr case: the reprojection matrix should be (ViewProj *
    // invViewProj) = I, so any current-frame clip point reprojects to itself.
    // Pins down the multiplication order: a swapped product would only pass
    // the identity test above.
    glm::mat4 view = glm::lookAt(glm::vec3(3.0F, 2.0F, 5.0F),
                                 glm::vec3(0.0F, 0.0F, 0.0F),
                                 glm::vec3(0.0F, 1.0F, 0.0F));
    glm::mat4 proj = glm::perspective(glm::radians(60.0F),
                                      16.0F / 9.0F, 0.1F, 100.0F);
    glm::mat4 viewProj = proj * view;
    glm::mat4 invViewProj = glm::inverse(viewProj);

    glm::mat4 reproj = ComputeReprojectionMatrix(viewProj, invViewProj);

    glm::vec4 currClip(0.3F, -0.5F, 0.7F, 1.0F);
    glm::vec4 prevClip = reproj * currClip;
    EXPECT_NEAR(prevClip.x, currClip.x, kEps);
    EXPECT_NEAR(prevClip.y, currClip.y, kEps);
    EXPECT_NEAR(prevClip.z, currClip.z, kEps);
    EXPECT_NEAR(prevClip.w, currClip.w, kEps);
}

TEST(SsilMathTest, ReprojectionMovedCameraRecoversPrevClipOfStaticPoint) {
    // Static world point, camera translated between frames. Current clip
    // comes from the current ViewProj; applying the reprojection matrix must
    // recover what the previous ViewProj would have produced for the same
    // world point. This is the actual contract ssil_main.comp relies on to
    // fetch last frame's HDR colour at the right UV.
    glm::mat4 proj = glm::perspective(glm::radians(60.0F),
                                      16.0F / 9.0F, 0.1F, 100.0F);
    glm::mat4 prevView = glm::lookAt(glm::vec3(0.0F, 0.0F, 5.0F),
                                     glm::vec3(0.0F, 0.0F, 0.0F),
                                     glm::vec3(0.0F, 1.0F, 0.0F));
    glm::mat4 currView = glm::lookAt(glm::vec3(0.5F, 0.2F, 5.1F),
                                     glm::vec3(0.0F, 0.0F, 0.0F),
                                     glm::vec3(0.0F, 1.0F, 0.0F));
    glm::mat4 prevViewProj = proj * prevView;
    glm::mat4 currViewProj = proj * currView;
    glm::mat4 currInvViewProj = glm::inverse(currViewProj);

    glm::mat4 reproj = ComputeReprojectionMatrix(prevViewProj, currInvViewProj);

    glm::vec4 worldPoint(1.2F, -0.4F, 0.3F, 1.0F);
    glm::vec4 currClip = currViewProj * worldPoint;
    glm::vec4 expectedPrevClip = prevViewProj * worldPoint;
    glm::vec4 reprojectedPrevClip = reproj * currClip;

    EXPECT_NEAR(reprojectedPrevClip.x, expectedPrevClip.x, 1e-4F);
    EXPECT_NEAR(reprojectedPrevClip.y, expectedPrevClip.y, 1e-4F);
    EXPECT_NEAR(reprojectedPrevClip.z, expectedPrevClip.z, 1e-4F);
    EXPECT_NEAR(reprojectedPrevClip.w, expectedPrevClip.w, 1e-4F);
}

TEST(SsilMathTest, NormalRejectionPeaksAtOneWhenNormalsMatch) {
    // Matching normals (dot = 1) → full weight for any positive strength.
    // The sampled pixel is on the same surface as the shaded pixel, so its
    // radiance should contribute with no attenuation.
    glm::vec3 n(0.0F, 1.0F, 0.0F);
    EXPECT_NEAR(SsilNormalRejectionWeight(n, n, 1.0F), 1.0F, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(n, n, 2.0F), 1.0F, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(n, n, 8.0F), 1.0F, kEps);
}

TEST(SsilMathTest, NormalRejectionFallsToZeroAtNinetyDegrees) {
    // Perpendicular normals (dot = 0) → zero contribution. A wall and a
    // floor meeting at 90° shouldn't leak indirect light from one surface
    // into the shading of the other in screen-space.
    glm::vec3 a(0.0F, 1.0F, 0.0F);
    glm::vec3 b(1.0F, 0.0F, 0.0F);
    EXPECT_NEAR(SsilNormalRejectionWeight(a, b, 1.0F), 0.0F, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(a, b, 4.0F), 0.0F, kEps);
}

TEST(SsilMathTest, NormalRejectionRejectsBackFacing) {
    // Opposing normals (dot = -1) → zero. Back-facing samples would be
    // behind the surface from the shaded pixel's point of view and must
    // not contribute.
    glm::vec3 n(0.0F, 1.0F, 0.0F);
    glm::vec3 opposite(0.0F, -1.0F, 0.0F);
    EXPECT_NEAR(SsilNormalRejectionWeight(n, opposite, 1.0F), 0.0F, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(n, opposite, 2.0F), 0.0F, kEps);
}

TEST(SsilMathTest, NormalRejectionStrengthSharpensLobe) {
    // At a partial angle (45° → dot = sqrt(2)/2 ≈ 0.707), the weight equals
    // dot^strength. Higher strength narrows the acceptance lobe: the panel
    // slider exposes this as "normal rejection" so a tighter lobe = more
    // rejection of oblique samples.
    glm::vec3 a(0.0F, 1.0F, 0.0F);
    glm::vec3 b = glm::normalize(glm::vec3(1.0F, 1.0F, 0.0F));  // 45°
    float dot45 = std::sqrt(2.0F) / 2.0F;
    EXPECT_NEAR(SsilNormalRejectionWeight(a, b, 1.0F), dot45, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(a, b, 2.0F), dot45 * dot45, kEps);
    EXPECT_NEAR(SsilNormalRejectionWeight(a, b, 4.0F),
                std::pow(dot45, 4.0F), kEps);
}

}  // namespace
