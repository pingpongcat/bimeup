#include <renderer/Camera.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

Camera::Camera() {
    UpdatePosition();
}

void Camera::SetPerspective(float fovDeg, float aspect, float near, float far) {
    m_projection = glm::perspective(glm::radians(fovDeg), aspect, near, far);
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

}  // namespace bimeup::renderer
