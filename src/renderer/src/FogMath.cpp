#include <renderer/FogMath.h>

#include <algorithm>

namespace bimeup::renderer {

float ComputeFog(float viewZ, float start, float end) {
    float denom = end - start;
    if (denom <= 1e-6F) {
        return viewZ >= start ? 1.0F : 0.0F;
    }
    return std::clamp((viewZ - start) / denom, 0.0F, 1.0F);
}

}  // namespace bimeup::renderer
