#include <renderer/OutlineEdge.h>

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

float SobelMagnitude(const std::array<float, 9>& p) {
    float gx = -p[0] + p[2]
               - (2.0F * p[3]) + (2.0F * p[5])
               - p[6] + p[8];
    float gy = -p[0] - (2.0F * p[1]) - p[2]
               + p[6] + (2.0F * p[7]) + p[8];
    return std::sqrt((gx * gx) + (gy * gy));
}

std::uint8_t EdgeFromStencil(const std::array<std::uint8_t, 9>& p) {
    // RP.12b: bit 4 = "transparent surface" rides alongside the base id
    // (0/1/2). Mask it out before the max-reduction so a glass overlay can't
    // hijack the edge category (and an interior of "selected through glass"
    // still reduces to a uniform value with no edge).
    constexpr std::uint8_t kCategoryMask = 0x3U;
    std::uint8_t lo = p[0] & kCategoryMask;
    std::uint8_t hi = lo;
    for (std::uint8_t v : p) {
        std::uint8_t base = v & kCategoryMask;
        lo = std::min(lo, base);
        hi = std::max(hi, base);
    }
    return (lo == hi) ? static_cast<std::uint8_t>(0) : hi;
}

}  // namespace bimeup::renderer
