#include <renderer/FirstPersonController.h>

#include <renderer/Camera.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace bimeup::renderer {

namespace {
constexpr float kPitchMargin = 1e-3F;
}

void FirstPersonController::SetPosition(glm::vec3 position) {
    m_position = position;
}

void FirstPersonController::SetYawPitch(float yaw, float pitch) {
    m_yaw = yaw;
    const float limit = glm::half_pi<float>() - kPitchMargin;
    m_pitch = std::clamp(pitch, -limit, limit);
}

void FirstPersonController::Look(glm::vec2 mouseDelta, float sensitivity) {
    m_yaw += mouseDelta.x * sensitivity;
    m_pitch -= mouseDelta.y * sensitivity;  // screen-y grows downward
    const float limit = glm::half_pi<float>() - kPitchMargin;
    m_pitch = std::clamp(m_pitch, -limit, limit);
}

glm::vec3 FirstPersonController::GetForward() const {
    // Match Camera::UpdatePosition: forward = -(cos(p)sin(y), sin(p), cos(p)cos(y)).
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);
    const float sy = std::sin(m_yaw);
    const float cy = std::cos(m_yaw);
    return glm::vec3(-cp * sy, -sp, -cp * cy);
}

void FirstPersonController::Move(glm::vec3 localInput, float dt, float speed) {
    // Horizontal forward (yaw only, pitch ignored).
    const glm::vec3 forwardHoriz(-std::sin(m_yaw), 0.0F, -std::cos(m_yaw));
    const glm::vec3 right = glm::cross(forwardHoriz, glm::vec3(0.0F, 1.0F, 0.0F));
    const glm::vec3 up(0.0F, 1.0F, 0.0F);

    const glm::vec3 delta = (right * localInput.x) + (up * localInput.y) + (forwardHoriz * localInput.z);
    m_position += delta * (speed * dt);
}

void FirstPersonController::ApplyTo(Camera& camera) const {
    // Camera is an orbit camera: view = lookAt(position, target). With yaw/pitch
    // set, position = target + offset(yaw, pitch, distance). To make
    // camera.GetPosition() == m_position, pick target = m_position + forward*D.
    constexpr float kD = 1.0F;
    const glm::vec3 target = m_position + GetForward() * kD;
    camera.SetOrbitTarget(target);
    camera.SetDistance(kD);
    camera.SetYawPitch(m_yaw, m_pitch);
}

}  // namespace bimeup::renderer
