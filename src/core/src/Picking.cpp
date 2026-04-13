#include "core/Picking.h"

#include "core/EventBus.h"
#include "core/Events.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace bimeup::core {

scene::Ray ScreenPointToRay(glm::vec2 screenPos,
                            glm::vec2 screenSize,
                            const glm::mat4& view,
                            const glm::mat4& proj) {
    float ndcX = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y) / screenSize.y;

    glm::mat4 invViewProj = glm::inverse(proj * view);

    glm::vec4 nearPoint = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    scene::Ray ray;
    ray.origin = glm::vec3(nearPoint);
    ray.direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    return ray;
}

bool PickElement(glm::vec2 screenPos,
                 glm::vec2 screenSize,
                 const glm::mat4& view,
                 const glm::mat4& proj,
                 const scene::Scene& scene,
                 std::span<const scene::SceneMesh> meshes,
                 EventBus& bus,
                 bool additive) {
    scene::Ray ray = ScreenPointToRay(screenPos, screenSize, view, proj);
    auto hit = scene::RaycastScene(ray, scene, meshes);
    if (!hit.has_value()) {
        return false;
    }

    // Translate NodeId (dense scene index) into the IFC expressId carried by
    // the owning SceneNode — the rest of the app (PropertyPanel, HierarchyPanel,
    // SelectionBridge) keys selection by expressId.
    uint32_t expressId = scene.GetNode(hit->nodeId).expressId;
    bus.Publish(ElementSelected{.expressId = expressId, .additive = additive});
    return true;
}

std::optional<scene::NodeId> HoverElement(glm::vec2 screenPos,
                                          glm::vec2 screenSize,
                                          const glm::mat4& view,
                                          const glm::mat4& proj,
                                          const scene::Scene& scene,
                                          std::span<const scene::SceneMesh> meshes,
                                          EventBus& bus) {
    scene::Ray ray = ScreenPointToRay(screenPos, screenSize, view, proj);
    auto hit = scene::RaycastScene(ray, scene, meshes);

    ElementHovered event;
    std::optional<scene::NodeId> result;
    if (hit.has_value()) {
        event.expressId = scene.GetNode(hit->nodeId).expressId;
        result = hit->nodeId;
    }
    bus.Publish(event);
    return result;
}

}  // namespace bimeup::core
