#include <renderer/SsaoKernel.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace bimeup::renderer {

namespace {

// Quantise a saturated [0, 1] weight to a 2-bit lattice index in [0, 3].
// Symmetric rounding via +0.5 → boundaries at 1/6, 3/6, 5/6 so lattice points
// {0, 1/3, 2/3, 1} land exactly on {0, 1, 2, 3}.
std::uint32_t Quantise2Bit(float v) {
    float clamped = std::clamp(v, 0.0F, 1.0F);
    auto q = static_cast<std::uint32_t>(std::floor((clamped * 3.0F) + 0.5F));
    return std::min<std::uint32_t>(q, 3U);
}

}  // namespace

std::vector<glm::vec3> GenerateHemisphereKernel(std::size_t count,
                                                std::uint32_t seed) {
    std::vector<glm::vec3> samples;
    samples.reserve(count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uniformXY(-1.0F, 1.0F);
    std::uniform_real_distribution<float> uniformZ(0.0F, 1.0F);
    std::uniform_real_distribution<float> uniformLen(0.0F, 1.0F);

    for (std::size_t i = 0; i < count; ++i) {
        glm::vec3 dir{uniformXY(rng), uniformXY(rng), uniformZ(rng)};
        dir = glm::normalize(dir);
        dir *= uniformLen(rng);
        // Quadratic falloff: t in [0.1, 1.0] scaled by (i/N)^2 — early samples
        // cluster near the origin for contact AO, later samples reach out for
        // wider occlusion.
        float t = static_cast<float>(i) / static_cast<float>(count);
        float scale = 0.1F + (0.9F * t * t);
        dir *= scale;
        samples.push_back(dir);
    }
    return samples;
}

std::uint8_t PackEdges(float left, float right, float top, float bottom) {
    std::uint32_t qL = Quantise2Bit(left);
    std::uint32_t qR = Quantise2Bit(right);
    std::uint32_t qT = Quantise2Bit(top);
    std::uint32_t qB = Quantise2Bit(bottom);
    std::uint32_t packed = qL | (qR << 2U) | (qT << 4U) | (qB << 6U);
    return static_cast<std::uint8_t>(packed);
}

glm::vec4 UnpackEdges(std::uint8_t packed) {
    constexpr float kInv3 = 1.0F / 3.0F;
    float l = static_cast<float>((packed >> 0U) & 0x3U) * kInv3;
    float r = static_cast<float>((packed >> 2U) & 0x3U) * kInv3;
    float t = static_cast<float>((packed >> 4U) & 0x3U) * kInv3;
    float b = static_cast<float>((packed >> 6U) & 0x3U) * kInv3;
    return {l, r, t, b};
}

}  // namespace bimeup::renderer
