#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

enum class RenderMode {
    Shaded,
    Wireframe,
};

inline VkPolygonMode GetPolygonMode(RenderMode mode) {
    switch (mode) {
        case RenderMode::Wireframe:
            return VK_POLYGON_MODE_LINE;
        case RenderMode::Shaded:
        default:
            return VK_POLYGON_MODE_FILL;
    }
}

}  // namespace bimeup::renderer
