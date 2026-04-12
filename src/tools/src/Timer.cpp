#include <tools/Timer.h>
#include <numeric>

namespace bimeup::tools {

Timer::Timer()
    : m_start(Clock::now())
    , m_lastTick(m_start) {}

void Timer::Tick() {
    auto now = Clock::now();
    m_deltaTime = std::chrono::duration<float>(now - m_lastTick).count();
    m_lastTick = now;
    ++m_frameCount;

    // Store in rolling window
    m_frameTimes[m_frameTimeIndex] = m_deltaTime;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % kFpsWindowSize;
    if (m_frameTimeFilled < kFpsWindowSize) {
        ++m_frameTimeFilled;
    }

    // Compute FPS from rolling average
    float sum = std::accumulate(m_frameTimes.begin(),
                                m_frameTimes.begin() + static_cast<ptrdiff_t>(m_frameTimeFilled),
                                0.0F);
    if (sum > 0.0F) {
        m_fps = static_cast<float>(m_frameTimeFilled) / sum;
    }
}

float Timer::GetDeltaTime() const {
    return m_deltaTime;
}

float Timer::GetElapsedTime() const {
    return std::chrono::duration<float>(Clock::now() - m_start).count();
}

float Timer::GetFPS() const {
    return m_fps;
}

uint64_t Timer::GetFrameCount() const {
    return m_frameCount;
}

} // namespace bimeup::tools
