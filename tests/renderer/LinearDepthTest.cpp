#include <gtest/gtest.h>

#include <renderer/LinearDepth.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace {

using bimeup::renderer::LinearizeDepth;
using bimeup::renderer::ReconstructViewPosFromDepth;

constexpr float kEps = 1e-4F;

// Explicit right-handed Vulkan-style perspective (depth range [0, 1], camera
// looks toward -z). Matches the math the GPU linearization shader will use;
// keeping it local to the test so we don't depend on glm's default which is
// GL-style [-1, 1]. Caller may then multiply [1][1] by -1 to apply Vulkan's UV
// y-flip.
glm::mat4 MakeVulkanPerspective(float fovYRad, float aspect, float nearZ, float farZ) {
    float tanHalfFovy = std::tan(fovYRad * 0.5F);
    glm::mat4 m(0.0F);
    m[0][0] = 1.0F / (aspect * tanHalfFovy);
    m[1][1] = 1.0F / tanHalfFovy;
    m[2][2] = farZ / (nearZ - farZ);
    m[2][3] = -1.0F;
    m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
    return m;
}

TEST(LinearDepthTest, LinearizeAtNearPlane) {
    // z_nl = 0 → linear depth = near
    EXPECT_NEAR(LinearizeDepth(0.0F, 0.1F, 100.0F), 0.1F, kEps);
    EXPECT_NEAR(LinearizeDepth(0.0F, 0.5F, 500.0F), 0.5F, kEps);
}

TEST(LinearDepthTest, LinearizeAtFarPlane) {
    // z_nl = 1 → linear depth = far
    EXPECT_NEAR(LinearizeDepth(1.0F, 0.1F, 100.0F), 100.0F, kEps);
    EXPECT_NEAR(LinearizeDepth(1.0F, 0.5F, 500.0F), 500.0F, kEps);
}

TEST(LinearDepthTest, LinearizeIsMonotonicIncreasing) {
    // As z_nl sweeps [0, 1], linear depth must strictly increase (with a
    // hyperbolic slope). Catches accidental sign flips.
    constexpr int kSteps = 64;
    float nearZ = 0.1F;
    float farZ = 100.0F;
    float prev = LinearizeDepth(0.0F, nearZ, farZ);
    for (int i = 1; i <= kSteps; ++i) {
        float z_nl = static_cast<float>(i) / static_cast<float>(kSteps);
        float lin = LinearizeDepth(z_nl, nearZ, farZ);
        EXPECT_GT(lin, prev) << "step " << i << " z_nl=" << z_nl;
        prev = lin;
    }
}

TEST(LinearDepthTest, LinearizeRoundTripAgainstMatrix) {
    // Push a known view-space point through the Vulkan perspective matrix,
    // divide by w, then linearize — should recover the input -z (view-space
    // distance).
    glm::mat4 proj = MakeVulkanPerspective(glm::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
    for (float d : {0.5F, 1.0F, 5.0F, 25.0F, 75.0F}) {
        glm::vec4 view{0.0F, 0.0F, -d, 1.0F};
        glm::vec4 clip = proj * view;
        float z_nl = clip.z / clip.w;
        float lin = LinearizeDepth(z_nl, 0.1F, 100.0F);
        EXPECT_NEAR(lin, d, 1e-3F) << "d=" << d << " z_nl=" << z_nl;
    }
}

TEST(LinearDepthTest, ReconstructCenterOfScreenHasZeroXY) {
    // uv=(0.5, 0.5) maps to ndc=(0, 0) — the reconstructed view position
    // must lie on the -z axis regardless of the projection's FOV/aspect/flip.
    glm::mat4 proj = MakeVulkanPerspective(glm::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec3 p = ReconstructViewPosFromDepth({0.5F, 0.5F}, 10.0F, invProj);
    EXPECT_NEAR(p.x, 0.0F, kEps);
    EXPECT_NEAR(p.y, 0.0F, kEps);
    EXPECT_NEAR(p.z, -10.0F, kEps);
}

TEST(LinearDepthTest, ReconstructRoundTripKnownPoint) {
    // Project a known view-space point, compute its UV + linear depth, then
    // reconstruct — should recover the original position.
    glm::mat4 proj = MakeVulkanPerspective(glm::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
    glm::mat4 invProj = glm::inverse(proj);
    struct Sample {
        glm::vec3 view;
    };
    Sample samples[] = {
        {{ 1.5F,  0.8F,  -5.0F}},
        {{-2.0F,  1.2F, -10.0F}},
        {{ 0.3F, -0.7F,  -2.0F}},
        {{ 4.0F,  2.0F, -20.0F}},
    };
    for (const auto& s : samples) {
        glm::vec4 clip = proj * glm::vec4(s.view, 1.0F);
        glm::vec3 ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
        glm::vec2 uv{(ndc.x + 1.0F) * 0.5F, (ndc.y + 1.0F) * 0.5F};
        float linearDepth = -s.view.z;
        glm::vec3 rec = ReconstructViewPosFromDepth(uv, linearDepth, invProj);
        EXPECT_NEAR(rec.x, s.view.x, 1e-3F) << "view=" << s.view.x << "," << s.view.y << "," << s.view.z;
        EXPECT_NEAR(rec.y, s.view.y, 1e-3F);
        EXPECT_NEAR(rec.z, s.view.z, 1e-3F);
    }
}

TEST(LinearDepthTest, ReconstructRoundTripYFlippedProjection) {
    // bimeup's live path flips proj[1][1] after glm::perspective to match
    // Vulkan UV (y=0 at top). The reconstruction helper must still round-trip
    // when given that exact inverse.
    glm::mat4 proj = MakeVulkanPerspective(glm::radians(50.0F), 4.0F / 3.0F, 0.1F, 100.0F);
    proj[1][1] *= -1.0F;
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec3 view{0.7F, -0.4F, -8.0F};
    glm::vec4 clip = proj * glm::vec4(view, 1.0F);
    glm::vec3 ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
    glm::vec2 uv{(ndc.x + 1.0F) * 0.5F, (ndc.y + 1.0F) * 0.5F};
    glm::vec3 rec = ReconstructViewPosFromDepth(uv, -view.z, invProj);
    EXPECT_NEAR(rec.x, view.x, 1e-3F);
    EXPECT_NEAR(rec.y, view.y, 1e-3F);
    EXPECT_NEAR(rec.z, view.z, 1e-3F);
}

TEST(LinearDepthTest, ReconstructScalesWithLinearDepth) {
    // For a fixed UV, doubling linearDepth doubles |viewPos|. Catches
    // a bug where the helper mixed linearDepth with non-linear depth.
    glm::mat4 proj = MakeVulkanPerspective(glm::radians(60.0F), 1.0F, 0.1F, 100.0F);
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec2 uv{0.25F, 0.75F};
    glm::vec3 a = ReconstructViewPosFromDepth(uv, 5.0F, invProj);
    glm::vec3 b = ReconstructViewPosFromDepth(uv, 10.0F, invProj);
    EXPECT_NEAR(b.x, 2.0F * a.x, 1e-3F);
    EXPECT_NEAR(b.y, 2.0F * a.y, 1e-3F);
    EXPECT_NEAR(b.z, 2.0F * a.z, 1e-3F);
    EXPECT_NEAR(a.z, -5.0F, 1e-3F);
    EXPECT_NEAR(b.z, -10.0F, 1e-3F);
}

}  // namespace
