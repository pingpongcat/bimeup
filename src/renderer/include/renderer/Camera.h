#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

class Camera {
public:
    Camera();

    void SetPerspective(float fovDeg, float aspect, float near, float far);
    void SetOrbitTarget(glm::vec3 target);
    void SetDistance(float distance);
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void Pan(glm::vec2 delta);

    [[nodiscard]] glm::mat4 GetViewMatrix() const;
    [[nodiscard]] glm::mat4 GetProjectionMatrix() const;
    [[nodiscard]] glm::vec3 GetPosition() const;
    [[nodiscard]] glm::vec3 GetForward() const;

private:
    void UpdatePosition();

    glm::vec3 m_target{0.0F, 0.0F, 0.0F};
    float m_distance = 5.0F;
    float m_yaw = 0.0F;
    float m_pitch = 0.3F;

    static constexpr float kMinDistance = 0.1F;
    static constexpr float kMaxDistance = 1000.0F;
    static constexpr float kMinPitch = -1.5F;
    static constexpr float kMaxPitch = 1.5F;

    glm::vec3 m_position{0.0F};
    glm::mat4 m_projection{1.0F};
};

}  // namespace bimeup::renderer
