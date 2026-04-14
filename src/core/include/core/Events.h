#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace bimeup::core {

struct ElementSelected {
    uint32_t expressId = 0;
    bool additive = false;
};

struct SelectionCleared {};

struct ElementHovered {
    std::optional<uint32_t> expressId;
};

struct ModelLoaded {
    std::string path;
    uint32_t elementCount = 0;
};

struct ViewChanged {
    glm::mat4 viewMatrix{1.0f};
    glm::vec3 cameraPosition{0.0f};
};

}  // namespace bimeup::core
