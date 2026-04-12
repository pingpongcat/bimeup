#pragma once

#include <array>
#include <chrono>
#include <cstdint>

namespace bimeup::tools {

class Timer {
public:
    Timer();

    /// Call once per frame to update delta time and frame count.
    void Tick();

    /// Seconds elapsed since the last Tick().
    [[nodiscard]] float GetDeltaTime() const;

    /// Seconds elapsed since Timer construction.
    [[nodiscard]] float GetElapsedTime() const;

    /// Frames per second, smoothed over a rolling window.
    [[nodiscard]] float GetFPS() const;

    /// Total number of Tick() calls.
    [[nodiscard]] uint64_t GetFrameCount() const;

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_start;
    TimePoint m_lastTick;
    float m_deltaTime = 0.0F;
    float m_fps = 0.0F;
    uint64_t m_frameCount = 0;

    // Rolling window for FPS smoothing
    static constexpr size_t kFpsWindowSize = 60;
    std::array<float, kFpsWindowSize> m_frameTimes = {};
    size_t m_frameTimeIndex = 0;
    size_t m_frameTimeFilled = 0;
};

} // namespace bimeup::tools
