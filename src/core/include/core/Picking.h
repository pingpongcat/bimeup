#pragma once

#include <optional>
#include <span>

#include <glm/glm.hpp>

#include <scene/Raycast.h>
#include <scene/Scene.h>
#include <scene/SceneMesh.h>

namespace bimeup::core {

class EventBus;

/// Build a world-space ray from a screen-space pixel coordinate.
/// screenPos uses top-left origin (y grows downward).
scene::Ray ScreenPointToRay(glm::vec2 screenPos,
                            glm::vec2 screenSize,
                            const glm::mat4& view,
                            const glm::mat4& proj);

/// Cast a ray from the screen position, raycast the scene, and publish
/// an ElementSelected event on hit. Returns true if something was hit.
bool PickElement(glm::vec2 screenPos,
                 glm::vec2 screenSize,
                 const glm::mat4& view,
                 const glm::mat4& proj,
                 const scene::Scene& scene,
                 std::span<const scene::SceneMesh> meshes,
                 EventBus& bus,
                 bool additive);

/// Cast a ray from the screen position and publish an ElementHovered
/// event with the hit node id (or nullopt on miss). Returns the hovered id.
std::optional<scene::NodeId> HoverElement(glm::vec2 screenPos,
                                          glm::vec2 screenSize,
                                          const glm::mat4& view,
                                          const glm::mat4& proj,
                                          const scene::Scene& scene,
                                          std::span<const scene::SceneMesh> meshes,
                                          EventBus& bus);

}  // namespace bimeup::core
