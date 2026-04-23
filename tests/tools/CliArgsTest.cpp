#include <gtest/gtest.h>
#include <tools/CliArgs.h>

#include <initializer_list>
#include <vector>

namespace {

bimeup::tools::CliArgs Parse(std::initializer_list<const char*> argv) {
    std::vector<const char*> v(argv);
    return bimeup::tools::ParseCliArgs(static_cast<int>(v.size()), v.data());
}

}  // namespace

TEST(CliArgsTest, NoArgsYieldsDefaults) {
    auto args = Parse({"bimeup"});
    EXPECT_TRUE(args.ok);
    EXPECT_FALSE(args.help);
    EXPECT_FALSE(args.deviceId.has_value());
    EXPECT_FALSE(args.ifcPath.has_value());
}

TEST(CliArgsTest, LongHelpFlagSetsHelp) {
    auto args = Parse({"bimeup", "--help"});
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.help);
}

TEST(CliArgsTest, ShortHelpFlagSetsHelp) {
    auto args = Parse({"bimeup", "-h"});
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.help);
}

TEST(CliArgsTest, DeviceIdSpaceFormSetsValue) {
    auto args = Parse({"bimeup", "--device-id", "2"});
    EXPECT_TRUE(args.ok);
    ASSERT_TRUE(args.deviceId.has_value());
    EXPECT_EQ(args.deviceId.value(), 2U);
}

TEST(CliArgsTest, DeviceIdEqualsFormSetsValue) {
    auto args = Parse({"bimeup", "--device-id=3"});
    EXPECT_TRUE(args.ok);
    ASSERT_TRUE(args.deviceId.has_value());
    EXPECT_EQ(args.deviceId.value(), 3U);
}

TEST(CliArgsTest, DeviceIdMissingValueIsError) {
    auto args = Parse({"bimeup", "--device-id"});
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.error.empty());
}

TEST(CliArgsTest, DeviceIdNonNumericIsError) {
    auto args = Parse({"bimeup", "--device-id", "abc"});
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.error.empty());
}

TEST(CliArgsTest, DeviceIdNegativeIsError) {
    auto args = Parse({"bimeup", "--device-id", "-1"});
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.error.empty());
}

TEST(CliArgsTest, PositionalIfcPathIsCaptured) {
    auto args = Parse({"bimeup", "model.ifc"});
    EXPECT_TRUE(args.ok);
    ASSERT_TRUE(args.ifcPath.has_value());
    EXPECT_EQ(args.ifcPath.value(), "model.ifc");
}

TEST(CliArgsTest, FlagsAndPositionalMixInEitherOrder) {
    auto a = Parse({"bimeup", "--device-id", "1", "model.ifc"});
    auto b = Parse({"bimeup", "model.ifc", "--device-id", "1"});
    EXPECT_TRUE(a.ok);
    EXPECT_TRUE(b.ok);
    EXPECT_EQ(a.deviceId, b.deviceId);
    EXPECT_EQ(a.ifcPath, b.ifcPath);
    EXPECT_EQ(a.ifcPath.value(), "model.ifc");
    EXPECT_EQ(a.deviceId.value(), 1U);
}

TEST(CliArgsTest, UnknownFlagIsError) {
    auto args = Parse({"bimeup", "--nope"});
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.error.empty());
}

TEST(CliArgsTest, MultiplePositionalsAreError) {
    auto args = Parse({"bimeup", "a.ifc", "b.ifc"});
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.error.empty());
}

TEST(CliArgsTest, HelpTextMentionsEveryFlag) {
    const std::string help = bimeup::tools::CliHelpText("bimeup");
    EXPECT_NE(help.find("--help"), std::string::npos);
    EXPECT_NE(help.find("-h"), std::string::npos);
    EXPECT_NE(help.find("--device-id"), std::string::npos);
    EXPECT_NE(help.find("bimeup"), std::string::npos);
}
