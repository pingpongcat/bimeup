#include "ifc/AsyncLoader.h"
#include "ifc/IfcModel.h"

#include <atomic>
#include <gtest/gtest.h>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace {
constexpr const char* kTestFile = TEST_DATA_DIR "/example.ifc";
}

TEST(AsyncLoader, LoadAsync_CompletesOnWorkerThread) {
    bimeup::ifc::AsyncLoader loader;
    std::atomic<std::thread::id> callbackThread{};

    auto fut = loader.LoadAsync(kTestFile, [&](float, std::string_view) {
        callbackThread.store(std::this_thread::get_id());
    });

    auto model = fut.get();
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(model->IsLoaded());
    EXPECT_NE(callbackThread.load(), std::this_thread::get_id());
}

TEST(AsyncLoader, LoadAsync_ProgressMonotonic_FirstZeroLastHundred) {
    bimeup::ifc::AsyncLoader loader;
    std::vector<float> percents;
    std::mutex mtx;

    auto fut = loader.LoadAsync(kTestFile, [&](float p, std::string_view) {
        std::lock_guard lk(mtx);
        percents.push_back(p);
    });

    auto model = fut.get();
    ASSERT_NE(model, nullptr);

    std::lock_guard lk(mtx);
    ASSERT_FALSE(percents.empty());
    EXPECT_FLOAT_EQ(percents.front(), 0.0F);
    EXPECT_FLOAT_EQ(percents.back(), 100.0F);
    for (size_t i = 1; i < percents.size(); ++i) {
        EXPECT_GE(percents[i], percents[i - 1]);
    }
}

TEST(AsyncLoader, LoadAsync_NullCallback_DoesNotCrash) {
    bimeup::ifc::AsyncLoader loader;
    auto fut = loader.LoadAsync(kTestFile, nullptr);
    auto model = fut.get();
    EXPECT_NE(model, nullptr);
}

TEST(AsyncLoader, LoadAsync_InvalidPath_ResolvesNullptr) {
    bimeup::ifc::AsyncLoader loader;
    auto fut = loader.LoadAsync("/nonexistent/path.ifc");
    auto model = fut.get();
    EXPECT_EQ(model, nullptr);
}

TEST(AsyncLoader, Cancel_SetsIsCancelled) {
    bimeup::ifc::AsyncLoader loader;
    EXPECT_FALSE(loader.IsCancelled());
    loader.Cancel();
    EXPECT_TRUE(loader.IsCancelled());
}

TEST(AsyncLoader, CancelDuringFirstProgress_ResolvesNullptr) {
    // The loader must check the cancel flag at every phase boundary. If we
    // request cancellation from inside the first progress callback (before
    // the parse phase begins), the returned future must resolve to nullptr.
    bimeup::ifc::AsyncLoader loader;
    std::atomic<bool> hooked{false};

    auto fut = loader.LoadAsync(kTestFile, [&](float, std::string_view phase) {
        if (phase == "starting" && !hooked.exchange(true)) {
            loader.Cancel();
        }
    });

    auto model = fut.get();
    EXPECT_EQ(model, nullptr);
    EXPECT_TRUE(loader.IsCancelled());
}

TEST(AsyncLoader, SecondLoad_ResetsCancelFlag) {
    bimeup::ifc::AsyncLoader loader;

    {
        auto fut = loader.LoadAsync(kTestFile, [&](float, std::string_view phase) {
            if (phase == "starting") loader.Cancel();
        });
        EXPECT_EQ(fut.get(), nullptr);
    }
    EXPECT_TRUE(loader.IsCancelled());

    auto fut = loader.LoadAsync(kTestFile);
    auto model = fut.get();
    EXPECT_NE(model, nullptr);
    EXPECT_FALSE(loader.IsCancelled());
}
