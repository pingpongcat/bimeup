#include <gtest/gtest.h>
#include <tools/Log.h>

TEST(LogTest, InitSucceeds) {
    EXPECT_NO_THROW(bimeup::tools::Log::Init("test"));
    bimeup::tools::Log::Shutdown();
}

TEST(LogTest, LogMacrosDoNotCrash) {
    bimeup::tools::Log::Init("test");

    EXPECT_NO_THROW({
        LOG_TRACE("trace message {}", 1);
        LOG_DEBUG("debug message {}", 2);
        LOG_INFO("info message {}", 3);
        LOG_WARN("warn message {}", 4);
        LOG_ERROR("error message {}", 5);
        LOG_FATAL("fatal message {}", 6);
    });

    bimeup::tools::Log::Shutdown();
}

TEST(LogTest, DoubleInitDoesNotCrash) {
    bimeup::tools::Log::Init("test");
    EXPECT_NO_THROW(bimeup::tools::Log::Init("test2"));
    bimeup::tools::Log::Shutdown();
}

TEST(LogTest, ShutdownWithoutInitDoesNotCrash) {
    EXPECT_NO_THROW(bimeup::tools::Log::Shutdown());
}
