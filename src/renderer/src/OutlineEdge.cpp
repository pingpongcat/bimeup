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
    std::uint8_t lo = p[0];
    std::uint8_t hi = p[0];
    for (std::uint8_t v : p) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    return (lo == hi) ? static_cast<std::uint8_t>(0) : hi;
}

}  // namespace bimeup::renderer
