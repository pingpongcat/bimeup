#include <gtest/gtest.h>
#include <renderer/Highlight.h>

using bimeup::renderer::ElementId;
using bimeup::renderer::Highlight;

TEST(HighlightTest, DefaultStateHasNoSelection) {
    Highlight h;
    EXPECT_EQ(h.Count(), 0u);
    EXPECT_FALSE(h.IsSelected(42));
    EXPECT_FALSE(h.GetOverrideColor(42).has_value());
}

TEST(HighlightTest, SelectMarksElement) {
    Highlight h;
    h.Select(7);
    EXPECT_TRUE(h.IsSelected(7));
    EXPECT_EQ(h.Count(), 1u);
    EXPECT_FALSE(h.IsSelected(8));
}

TEST(HighlightTest, SelectIsIdempotent) {
    Highlight h;
    h.Select(3);
    h.Select(3);
    EXPECT_EQ(h.Count(), 1u);
}

TEST(HighlightTest, SelectMultipleIds) {
    Highlight h;
    h.Select(1);
    h.Select(2);
    h.Select(3);
    EXPECT_EQ(h.Count(), 3u);
    EXPECT_TRUE(h.IsSelected(1));
    EXPECT_TRUE(h.IsSelected(2));
    EXPECT_TRUE(h.IsSelected(3));
}

TEST(HighlightTest, DeselectRemovesElement) {
    Highlight h;
    h.Select(5);
    h.Select(6);
    h.Deselect(5);
    EXPECT_FALSE(h.IsSelected(5));
    EXPECT_TRUE(h.IsSelected(6));
    EXPECT_EQ(h.Count(), 1u);
}

TEST(HighlightTest, DeselectMissingIdIsNoop) {
    Highlight h;
    h.Select(5);
    h.Deselect(99);
    EXPECT_EQ(h.Count(), 1u);
}

TEST(HighlightTest, ClearRemovesAll) {
    Highlight h;
    h.Select(1);
    h.Select(2);
    h.Clear();
    EXPECT_EQ(h.Count(), 0u);
    EXPECT_FALSE(h.IsSelected(1));
    EXPECT_FALSE(h.IsSelected(2));
}

TEST(HighlightTest, GetOverrideColorReturnsColorForSelected) {
    Highlight h;
    h.Select(42);
    auto color = h.GetOverrideColor(42);
    ASSERT_TRUE(color.has_value());
    EXPECT_EQ(*color, h.GetColor());
}

TEST(HighlightTest, GetOverrideColorReturnsEmptyForUnselected) {
    Highlight h;
    h.Select(1);
    EXPECT_FALSE(h.GetOverrideColor(2).has_value());
}

TEST(HighlightTest, CustomColorIsReturned) {
    glm::vec4 custom(0.2f, 0.3f, 0.9f, 1.0f);
    Highlight h(custom);
    h.Select(1);
    EXPECT_EQ(h.GetColor(), custom);
    EXPECT_EQ(*h.GetOverrideColor(1), custom);
}

TEST(HighlightTest, SetColorAffectsExistingSelections) {
    Highlight h;
    h.Select(1);
    glm::vec4 newColor(0.0f, 1.0f, 0.0f, 1.0f);
    h.SetColor(newColor);
    EXPECT_EQ(*h.GetOverrideColor(1), newColor);
}

TEST(HighlightTest, IdsExposesSelectionSet) {
    Highlight h;
    h.Select(10);
    h.Select(20);
    const auto& ids = h.Ids();
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_NE(ids.find(10), ids.end());
    EXPECT_NE(ids.find(20), ids.end());
}
