#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <string>

#include "core/Events.h"

namespace bimeup::core {

TEST(EventsTest, ElementSelectedHasExpressId) {
    ElementSelected event{.expressId = 42};
    EXPECT_EQ(event.expressId, 42u);
}

TEST(EventsTest, ElementSelectedCanCarryAdditiveFlag) {
    ElementSelected event{.expressId = 7, .additive = true};
    EXPECT_EQ(event.expressId, 7u);
    EXPECT_TRUE(event.additive);
}

TEST(EventsTest, ElementSelectedDefaultAdditiveFalse) {
    ElementSelected event{.expressId = 1};
    EXPECT_FALSE(event.additive);
}

TEST(EventsTest, ElementHoveredHasExpressId) {
    ElementHovered event{.expressId = 123};
    EXPECT_EQ(event.expressId, 123u);
}

TEST(EventsTest, ElementHoveredCanBeEmpty) {
    ElementHovered event{};
    EXPECT_FALSE(event.expressId.has_value());
}

TEST(EventsTest, ModelLoadedCarriesPathAndElementCount) {
    ModelLoaded event{.path = "/tmp/model.ifc", .elementCount = 1024};
    EXPECT_EQ(event.path, "/tmp/model.ifc");
    EXPECT_EQ(event.elementCount, 1024u);
}

TEST(EventsTest, ViewChangedCarriesViewMatrixAndPosition) {
    glm::mat4 view = glm::mat4(1.0f);
    glm::vec3 pos(1.0f, 2.0f, 3.0f);
    ViewChanged event{.viewMatrix = view, .cameraPosition = pos};
    EXPECT_EQ(event.viewMatrix, view);
    EXPECT_EQ(event.cameraPosition, pos);
}

}  // namespace bimeup::core
