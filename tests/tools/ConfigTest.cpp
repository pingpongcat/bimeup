#include <gtest/gtest.h>
#include <tools/Config.h>

TEST(ConfigTest, LoadFromStringParsesSections) {
    bimeup::tools::Config config;
    EXPECT_TRUE(config.LoadFromString(R"(
[window]
width = 1280
height = 720
title = Bimeup Viewer
fullscreen = false
fov = 75.5
)"));
}

TEST(ConfigTest, GetStringReturnsValue) {
    bimeup::tools::Config config;
    config.LoadFromString("[app]\nname = TestApp");
    EXPECT_EQ(config.GetString("app.name"), "TestApp");
}

TEST(ConfigTest, GetStringReturnsDefaultWhenKeyMissing) {
    bimeup::tools::Config config;
    config.LoadFromString("[app]\nname = TestApp");
    EXPECT_EQ(config.GetString("app.missing", "default"), "default");
}

TEST(ConfigTest, GetIntReturnsValue) {
    bimeup::tools::Config config;
    config.LoadFromString("[window]\nwidth = 1280");
    EXPECT_EQ(config.GetInt("window.width"), 1280);
}

TEST(ConfigTest, GetIntReturnsDefaultWhenKeyMissing) {
    bimeup::tools::Config config;
    config.LoadFromString("[window]\nwidth = 1280");
    EXPECT_EQ(config.GetInt("window.missing", 42), 42);
}

TEST(ConfigTest, GetFloatReturnsValue) {
    bimeup::tools::Config config;
    config.LoadFromString("[camera]\nfov = 75.5");
    EXPECT_FLOAT_EQ(config.GetFloat("camera.fov"), 75.5f);
}

TEST(ConfigTest, GetBoolReturnsTrue) {
    bimeup::tools::Config config;
    config.LoadFromString("[window]\nfullscreen = true");
    EXPECT_TRUE(config.GetBool("window.fullscreen"));
}

TEST(ConfigTest, GetBoolReturnsFalse) {
    bimeup::tools::Config config;
    config.LoadFromString("[window]\nfullscreen = false");
    EXPECT_FALSE(config.GetBool("window.fullscreen"));
}

TEST(ConfigTest, GetBoolReturnsDefaultWhenMissing) {
    bimeup::tools::Config config;
    config.LoadFromString("[window]\nfullscreen = false");
    EXPECT_TRUE(config.GetBool("window.missing", true));
}

TEST(ConfigTest, EmptyStringReturnsFalse) {
    bimeup::tools::Config config;
    EXPECT_FALSE(config.LoadFromString(""));
}

TEST(ConfigTest, KeysWithoutSectionUseGlobalSection) {
    bimeup::tools::Config config;
    config.LoadFromString("name = TestApp");
    EXPECT_EQ(config.GetString("name"), "TestApp");
}

TEST(ConfigTest, WhitespaceAroundValuesIsTrimmed) {
    bimeup::tools::Config config;
    config.LoadFromString("[app]\n  name  =  Bimeup Viewer  ");
    EXPECT_EQ(config.GetString("app.name"), "Bimeup Viewer");
}
