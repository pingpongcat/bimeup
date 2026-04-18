#include <ui/AxisSectionGizmo.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

using bimeup::ui::AxisDragDelta;
using bimeup::ui::ProjectWorldToScreen;

namespace {

constexpr float kTol = 1e-3F;

TEST(AxisSectionGizmoTest, ProjectWorldToScreenCentersOrigin) {
    const glm::mat4 id{1.0F};
    const auto screen =
        ProjectWorldToScreen(id, id, glm::vec3{0.0F}, glm::vec2{800.0F, 600.0F});
    ASSERT_TRUE(screen.has_value());
    EXPECT_NEAR(screen->x, 400.0F, kTol);
    EXPECT_NEAR(screen->y, 300.0F, kTol);
}

TEST(AxisSectionGizmoTest, ProjectWorldToScreenMovesRightAlongX) {
    const glm::mat4 id{1.0F};
    const auto screen =
        ProjectWorldToScreen(id, id, glm::vec3{0.5F, 0.0F, 0.0F},
                             glm::vec2{800.0F, 600.0F});
    ASSERT_TRUE(screen.has_value());
    EXPECT_NEAR(screen->x, 600.0F, kTol);
    EXPECT_NEAR(screen->y, 300.0F, kTol);
}

TEST(AxisSectionGizmoTest, ProjectWorldToScreenMovesUpAlongY) {
    const glm::mat4 id{1.0F};
    const auto screen =
        ProjectWorldToScreen(id, id, glm::vec3{0.0F, 0.5F, 0.0F},
                             glm::vec2{800.0F, 600.0F});
    ASSERT_TRUE(screen.has_value());
    EXPECT_NEAR(screen->x, 400.0F, kTol);
    EXPECT_NEAR(screen->y, 150.0F, kTol);  // Y flipped for screen-space
}

TEST(AxisSectionGizmoTest, ProjectWorldToScreenReturnsNulloptBehindCamera) {
    glm::mat4 proj{1.0F};
    proj[3][3] = 0.0F;
    proj[2][3] = 1.0F;  // clip.w = +z of world point → negative when behind
    const auto screen =
        ProjectWorldToScreen(glm::mat4{1.0F}, proj, glm::vec3{0.0F, 0.0F, -1.0F},
                             glm::vec2{800.0F, 600.0F});
    EXPECT_FALSE(screen.has_value());
}

TEST(AxisSectionGizmoTest, AxisDragDeltaColinearReturnsMouseOverScale) {
    const glm::vec2 start{100.0F, 100.0F};
    const glm::vec2 end{200.0F, 100.0F};  // 100 px per world unit along +X screen
    const glm::vec2 mouseDelta{50.0F, 0.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    ASSERT_TRUE(delta.has_value());
    EXPECT_NEAR(*delta, 0.5F, kTol);
}

TEST(AxisSectionGizmoTest, AxisDragDeltaPerpendicularReturnsZero) {
    const glm::vec2 start{100.0F, 100.0F};
    const glm::vec2 end{200.0F, 100.0F};
    const glm::vec2 mouseDelta{0.0F, 50.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    ASSERT_TRUE(delta.has_value());
    EXPECT_NEAR(*delta, 0.0F, kTol);
}

TEST(AxisSectionGizmoTest, AxisDragDeltaObliqueProjectsAlongAxis) {
    const glm::vec2 start{0.0F, 0.0F};
    const glm::vec2 end{100.0F, 0.0F};
    const glm::vec2 mouseDelta{30.0F, 40.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    ASSERT_TRUE(delta.has_value());
    EXPECT_NEAR(*delta, 0.30F, kTol);
}

TEST(AxisSectionGizmoTest, AxisDragDeltaReverseDirectionProducesNegative) {
    const glm::vec2 start{200.0F, 100.0F};
    const glm::vec2 end{100.0F, 100.0F};  // screen +axis points -X
    const glm::vec2 mouseDelta{50.0F, 0.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    ASSERT_TRUE(delta.has_value());
    EXPECT_NEAR(*delta, -0.5F, kTol);
}

TEST(AxisSectionGizmoTest, AxisDragDeltaShortAxisReturnsNullopt) {
    const glm::vec2 start{100.0F, 100.0F};
    const glm::vec2 end{103.0F, 103.0F};  // ~4.2 px, below 8 px threshold
    const glm::vec2 mouseDelta{50.0F, 0.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    EXPECT_FALSE(delta.has_value());
}

TEST(AxisSectionGizmoTest, AxisDragDeltaDiagonalAxisReturnsProjectedLength) {
    // Axis is a 45° diagonal 141.4 px long (per world unit). A purely
    // horizontal 100 px mouse drag projects to 70.7 px along the axis → 0.5
    // world units.
    const glm::vec2 start{0.0F, 0.0F};
    const glm::vec2 end{100.0F, 100.0F};
    const glm::vec2 mouseDelta{100.0F, 0.0F};
    const auto delta = AxisDragDelta(start, end, mouseDelta, 8.0F);
    ASSERT_TRUE(delta.has_value());
    EXPECT_NEAR(*delta, 0.5F, kTol);
}

}  // namespace
