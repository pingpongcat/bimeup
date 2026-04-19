#include "renderer/SmaaLut.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace bimeup {
namespace {

TEST(SmaaLutTest, AreaTexDimensionsMatchSmaa1xContract) {
    EXPECT_EQ(renderer::SmaaAreaTex::kWidth, 160U);
    EXPECT_EQ(renderer::SmaaAreaTex::kHeight, 560U);
    EXPECT_EQ(renderer::SmaaAreaTex::kChannels, 2U);
}

TEST(SmaaLutTest, SearchTexDimensionsMatchSmaa1xContract) {
    EXPECT_EQ(renderer::SmaaSearchTex::kWidth, 64U);
    EXPECT_EQ(renderer::SmaaSearchTex::kHeight, 16U);
    EXPECT_EQ(renderer::SmaaSearchTex::kChannels, 1U);
}

TEST(SmaaLutTest, AreaTexSizeBytesMatchesWidthTimesHeightTimesChannels) {
    EXPECT_EQ(
        renderer::SmaaAreaTex::kSizeBytes,
        renderer::SmaaAreaTex::kWidth * renderer::SmaaAreaTex::kHeight *
            renderer::SmaaAreaTex::kChannels);
    EXPECT_EQ(renderer::SmaaAreaTex::kSizeBytes, 179200U);
}

TEST(SmaaLutTest, SearchTexSizeBytesMatchesWidthTimesHeightTimesChannels) {
    EXPECT_EQ(
        renderer::SmaaSearchTex::kSizeBytes,
        renderer::SmaaSearchTex::kWidth * renderer::SmaaSearchTex::kHeight *
            renderer::SmaaSearchTex::kChannels);
    EXPECT_EQ(renderer::SmaaSearchTex::kSizeBytes, 1024U);
}

TEST(SmaaLutTest, AreaTexDataIsNonZero) {
    const auto* data = renderer::SmaaAreaTex::Data();
    ASSERT_NE(data, nullptr);
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < renderer::SmaaAreaTex::kSizeBytes; ++i) {
        sum += data[i];
    }
    EXPECT_GT(sum, 0U);
}

TEST(SmaaLutTest, SearchTexDataIsNonZero) {
    const auto* data = renderer::SmaaSearchTex::Data();
    ASSERT_NE(data, nullptr);
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < renderer::SmaaSearchTex::kSizeBytes; ++i) {
        sum += data[i];
    }
    EXPECT_GT(sum, 0U);
}

}  // namespace
}  // namespace bimeup
