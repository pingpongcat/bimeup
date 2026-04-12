#include <tools/Timer.h>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace bimeup::tools;

TEST(TimerTest, InitialState) {
    Timer timer;
    EXPECT_FLOAT_EQ(timer.GetDeltaTime(), 0.0f);
    EXPECT_EQ(timer.GetFrameCount(), 0u);
    EXPECT_FLOAT_EQ(timer.GetFPS(), 0.0f);
}

TEST(TimerTest, ElapsedTimeMonotonicallyIncreases) {
    Timer timer;
    float prev = timer.GetElapsedTime();

    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timer.Tick();
        float now = timer.GetElapsedTime();
        EXPECT_GT(now, prev) << "Elapsed time must increase on tick " << i;
        prev = now;
    }
}

TEST(TimerTest, DeltaTimeIsPositiveAfterTick) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.Tick();
    EXPECT_GT(timer.GetDeltaTime(), 0.0f);
}

TEST(TimerTest, FrameCountIncrements) {
    Timer timer;
    EXPECT_EQ(timer.GetFrameCount(), 0u);

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 1u);

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 2u);

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 3u);
}

TEST(TimerTest, FPSComputesCorrectly) {
    Timer timer;

    // Simulate ~100 FPS (10ms per frame) for enough ticks to fill the window
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timer.Tick();
    }

    float fps = timer.GetFPS();
    // With 10ms sleeps, expect roughly 100 FPS.
    // Allow wide tolerance for CI/slow machines: 50-200 FPS
    EXPECT_GT(fps, 30.0f);
    EXPECT_LT(fps, 200.0f);
}

TEST(TimerTest, DeltaTimeReflectsSleepDuration) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.Tick();

    float dt = timer.GetDeltaTime();
    // Should be roughly 50ms = 0.05s, allow 20-150ms range for scheduler jitter
    EXPECT_GT(dt, 0.02f);
    EXPECT_LT(dt, 0.15f);
}
