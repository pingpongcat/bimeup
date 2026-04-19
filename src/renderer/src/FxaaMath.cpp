#include <renderer/FxaaMath.h>

#include <algorithm>

namespace bimeup::renderer {

float FxaaLuminance(const glm::vec3& rgb) {
    return 0.2126F * rgb.r + 0.7152F * rgb.g + 0.0722F * rgb.b;
}

float FxaaLocalContrast(float lumaCenter, float lumaNorth, float lumaSouth,
                        float lumaEast, float lumaWest) {
    float lumaMax = std::max({lumaCenter, lumaNorth, lumaSouth,
                              lumaEast, lumaWest});
    float lumaMin = std::min({lumaCenter, lumaNorth, lumaSouth,
                              lumaEast, lumaWest});
    return lumaMax - lumaMin;
}

bool FxaaIsEdge(float lumaCenter, float lumaNorth, float lumaSouth,
                float lumaEast, float lumaWest,
                float edgeThreshold, float edgeThresholdMin) {
    float lumaMax = std::max({lumaCenter, lumaNorth, lumaSouth,
                              lumaEast, lumaWest});
    float lumaMin = std::min({lumaCenter, lumaNorth, lumaSouth,
                              lumaEast, lumaWest});
    float range = lumaMax - lumaMin;
    float gate = std::max(edgeThresholdMin, lumaMax * edgeThreshold);
    return range >= gate;
}

}  // namespace bimeup::renderer
