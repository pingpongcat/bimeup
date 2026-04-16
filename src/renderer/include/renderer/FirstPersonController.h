#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

class Camera;

/// First-person camera controller. Independent of `Camera` — call `ApplyTo`
/// once per frame to push position+orientation into the orbit camera.
///
/// Conventions match `Camera`: yaw=0,pitch=0 looks along -Z. Increasing yaw
/// turns right; increasing pitch tilts the gaze upward.
class FirstPersonController {
public:
    FirstPersonController() = default;

    void SetPosition(glm::vec3 position);
    void SetYawPitch(float yaw, float pitch);

    /// Mouse-look update. `mouseDelta` is in pixels (screen convention: x right,
    /// y down). Sensitivity is radians per pixel. Pitch is clamped to
    /// [-π/2 + ε, π/2 - ε] to avoid the lookAt singularity.
    void Look(glm::vec2 mouseDelta, float sensitivity);

    /// Translation update. `localInput` channels:
    ///   .x = strafe (right positive),
    ///   .y = vertical (up positive, world Y),
    ///   .z = forward (positive moves toward gaze).
    /// Forward/right are derived from yaw only — pitch does not raise/lower
    /// the walker, matching standard FPS feel.
    void Move(glm::vec3 localInput, float dt, float speed);

    [[nodiscard]] glm::vec3 GetPosition() const { return m_position; }
    [[nodiscard]] float GetYaw() const { return m_yaw; }
    [[nodiscard]] float GetPitch() const { return m_pitch; }

    /// Gaze direction including pitch, normalized.
    [[nodiscard]] glm::vec3 GetForward() const;

    /// Push position and orientation into the orbit camera so its world position
    /// equals `GetPosition()` and it looks along `GetForward()`.
    void ApplyTo(Camera& camera) const;

private:
    glm::vec3 m_position{0.0F};
    float m_yaw = 0.0F;
    float m_pitch = 0.0F;
};

}  // namespace bimeup::renderer
