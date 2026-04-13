#include <renderer/Camera.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

Camera::Camera() {
    UpdatePosition();
}

void Camera::SetPerspective(float fovDeg, float aspect, float near, float far) {
    m_perspFovDeg = fovDeg;
    m_perspAspect = aspect;
    m_perspNear = near;
    m_perspFar = far;
    m_projectionMode = ProjectionMode::Perspective;
    RebuildProjection();
}

void Camera::SetOrthographic(float height, float aspect, float near, float far) {
    m_orthoHeight = height;
    m_orthoAspect = aspect;
    m_orthoNear = near;
    m_orthoFar = far;
    m_projectionMode = ProjectionMode::Orthographic;
    RebuildProjection();
}

void Camera::ToggleProjection() {
    if (m_projectionMode == ProjectionMode::Perspective) {
        float height = 2.0F * m_distance * std::tan(glm::radians(m_perspFovDeg) * 0.5F);
        SetOrthographic(height, m_perspAspect, m_perspNear, m_perspFar);
    } else {
        SetPerspective(m_perspFovDeg, m_orthoAspect, m_orthoNear, m_orthoFar);
    }
}

void Camera::RebuildProjection() {
    if (m_projectionMode == ProjectionMode::Perspective) {
        m_projection = glm::perspective(glm::radians(m_perspFovDeg), m_perspAspect,
                                        m_perspNear, m_perspFar);
    } else {
        float halfH = m_orthoHeight * 0.5F;
        float halfW = halfH * m_orthoAspect;
        // Use ZO (zero-to-one) ortho to match Vulkan's clip Z range [0, 1].
        // glm::ortho defaults to OpenGL's [-1, 1] which clips everything in
        // front of the ortho mid-plane in Vulkan.
        m_projection = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, m_orthoNear, m_orthoFar);
    }
    // Vulkan clip space has inverted Y compared to OpenGL
    m_projection[1][1] *= -1.0F;
}

void Camera::SetOrbitTarget(glm::vec3 target) {
    m_target = target;
    UpdatePosition();
}

void Camera::SetDistance(float distance) {
    m_distance = std::clamp(distance, kMinDistance, kMaxDistance);
    UpdatePosition();
}

void Camera::Orbit(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;
    m_pitch = std::clamp(m_pitch, kMinPitch, kMaxPitch);
    UpdatePosition();
}

void Camera::Zoom(float delta) {
    m_distance += delta;
    m_distance = std::clamp(m_distance, kMinDistance, kMaxDistance);
    UpdatePosition();
}

void Camera::Frame(const glm::vec3& min, const glm::vec3& max) {
    if (min.x > max.x || min.y > max.y || min.z > max.z) {
        return;
    }
    glm::vec3 size = max - min;
    float maxDim = std::max({size.x, size.y, size.z});
    SetOrbitTarget((min + max) * 0.5F);
    SetDistance(std::max(maxDim * 1.5F, 0.5F));
}

void Camera::SetAxisView(AxisView view) {
    // Use a pitch just under π/2 for top/bottom to avoid the lookAt
    // singularity when the view direction aligns with the world up vector.
    constexpr float kHalfPi = 1.57079632679F;
    constexpr float kTopPitch = kHalfPi - 1e-3F;
    switch (view) {
        case AxisView::Front:
            m_yaw = 0.0F;
            m_pitch = 0.0F;
            break;
        case AxisView::Back:
            m_yaw = kHalfPi * 2.0F;
            m_pitch = 0.0F;
            break;
        case AxisView::Right:
            m_yaw = kHalfPi;
            m_pitch = 0.0F;
            break;
        case AxisView::Left:
            m_yaw = -kHalfPi;
            m_pitch = 0.0F;
            break;
        case AxisView::Top:
            m_pitch = kTopPitch;
            break;
        case AxisView::Bottom:
            m_pitch = -kTopPitch;
            break;
    }
    UpdatePosition();
}

void Camera::SetYawPitch(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, kMinPitch, kMaxPitch);
    UpdatePosition();
}

void Camera::Pan(glm::vec2 delta) {
    glm::vec3 forward = GetForward();
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    m_target += right * delta.x + up * delta.y;
    UpdatePosition();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_position, m_target, glm::vec3(0.0F, 1.0F, 0.0F));
}

glm::mat4 Camera::GetProjectionMatrix() const {
    return m_projection;
}

glm::vec3 Camera::GetPosition() const {
    return m_position;
}

glm::vec3 Camera::GetForward() const {
    return glm::normalize(m_target - m_position);
}

void Camera::UpdatePosition() {
    float x = m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    float y = m_distance * std::sin(m_pitch);
    float z = m_distance * std::cos(m_pitch) * std::cos(m_yaw);
    m_position = m_target + glm::vec3(x, y, z);
}

glm::vec2 YawPitchFromForward(const glm::vec3& forward) {
    glm::vec3 f = glm::normalize(forward);
    float pitch = std::asin(std::clamp(-f.y, -1.0F, 1.0F));
    float yaw = std::atan2(-f.x, -f.z);
    return {yaw, pitch};
}

}  // namespace bimeup::renderer
