#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

enum class ProjectionMode {
    Perspective,
    Orthographic,
};

class Camera {
public:
    Camera();

    void SetPerspective(float fovDeg, float aspect, float near, float far);
    void SetOrthographic(float height, float aspect, float near, float far);
    void ToggleProjection();
    void SetOrbitTarget(glm::vec3 target);
    void SetDistance(float distance);
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void Pan(glm::vec2 delta);

    // Fit the camera so the AABB [min,max] is visible. Sets the orbit pivot to
    // the bounds center and the distance to `max(1.5 * largest_dim, 0.5)`.
    // Orbit angles are preserved. If max < min on any axis, this is a no-op.
    void Frame(const glm::vec3& min, const glm::vec3& max);

    [[nodiscard]] glm::mat4 GetViewMatrix() const;
    [[nodiscard]] glm::mat4 GetProjectionMatrix() const;
    [[nodiscard]] glm::vec3 GetPosition() const;
    [[nodiscard]] glm::vec3 GetForward() const;
    [[nodiscard]] ProjectionMode GetProjectionMode() const { return m_projectionMode; }
    [[nodiscard]] bool IsOrthographic() const { return m_projectionMode == ProjectionMode::Orthographic; }

private:
    void UpdatePosition();
    void RebuildProjection();

    glm::vec3 m_target{0.0F, 0.0F, 0.0F};
    float m_distance = 5.0F;
    float m_yaw = 0.0F;
    float m_pitch = 0.3F;

    static constexpr float kMinDistance = 0.1F;
    static constexpr float kMaxDistance = 1000.0F;
    static constexpr float kMinPitch = -1.5F;
    static constexpr float kMaxPitch = 1.5F;

    ProjectionMode m_projectionMode = ProjectionMode::Perspective;
    float m_perspFovDeg = 45.0F;
    float m_perspAspect = 1.0F;
    float m_perspNear = 0.1F;
    float m_perspFar = 100.0F;
    float m_orthoHeight = 10.0F;
    float m_orthoAspect = 1.0F;
    float m_orthoNear = 0.1F;
    float m_orthoFar = 100.0F;

    glm::vec3 m_position{0.0F};
    glm::mat4 m_projection{1.0F};
};

}  // namespace bimeup::renderer
