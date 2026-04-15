#include <gtest/gtest.h>

#include <scene/Scene.h>
#include <ui/TypeVisibilityPanel.h>

#include <algorithm>

namespace {

using bimeup::scene::Scene;
using bimeup::scene::SceneNode;
using bimeup::ui::TypeVisibilityPanel;

Scene MakeThreeTypeScene() {
    Scene scene;
    SceneNode wall; wall.ifcType = "IfcWall"; scene.AddNode(std::move(wall));
    SceneNode slab; slab.ifcType = "IfcSlab"; scene.AddNode(std::move(slab));
    SceneNode space; space.ifcType = "IfcSpace"; scene.AddNode(std::move(space));
    SceneNode door; door.ifcType = "IfcDoor"; scene.AddNode(std::move(door));
    SceneNode wall2; wall2.ifcType = "IfcWall"; scene.AddNode(std::move(wall2));
    return scene;
}

TEST(TypeVisibilityPanelTest, HasPanelName) {
    TypeVisibilityPanel panel;
    EXPECT_STREQ(panel.GetName(), "Types");
}

TEST(TypeVisibilityPanelTest, DefaultEmpty) {
    TypeVisibilityPanel panel;
    EXPECT_TRUE(panel.GetTypes().empty());
    EXPECT_EQ(panel.GetScene(), nullptr);
}

TEST(TypeVisibilityPanelTest, SetSceneListsUniqueTypesSorted) {
    Scene scene = MakeThreeTypeScene();
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);

    const auto& types = panel.GetTypes();
    ASSERT_EQ(types.size(), 4u);
    EXPECT_EQ(types[0], "IfcDoor");
    EXPECT_EQ(types[1], "IfcSlab");
    EXPECT_EQ(types[2], "IfcSpace");
    EXPECT_EQ(types[3], "IfcWall");
}

TEST(TypeVisibilityPanelTest, SetTypeVisibleUpdatesScene) {
    Scene scene = MakeThreeTypeScene();
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);

    panel.SetTypeVisible("IfcWall", false);
    EXPECT_FALSE(panel.IsTypeVisible("IfcWall"));
    // Both IfcWall nodes should be hidden (ids 0 and 4).
    EXPECT_FALSE(scene.GetNode(0).visible);
    EXPECT_FALSE(scene.GetNode(4).visible);
    // Other types untouched.
    EXPECT_TRUE(scene.GetNode(1).visible);
    EXPECT_TRUE(scene.GetNode(3).visible);
}

TEST(TypeVisibilityPanelTest, ApplyDefaultsHidesNonVisualTypes) {
    Scene scene = MakeThreeTypeScene();
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);
    panel.ApplyDefaults();

    EXPECT_FALSE(panel.IsTypeVisible("IfcSpace"));
    EXPECT_TRUE(panel.IsTypeVisible("IfcWall"));
    EXPECT_TRUE(panel.IsTypeVisible("IfcSlab"));
    EXPECT_TRUE(panel.IsTypeVisible("IfcDoor"));

    // Scene node for the IfcSpace entry should be hidden.
    const auto& nodes = scene;
    EXPECT_FALSE(nodes.GetNode(2).visible);
}

TEST(TypeVisibilityPanelTest, ApplyDefaultsSkipsTypesNotPresent) {
    // Scene has no IfcSpace — ApplyDefaults should be a no-op for it.
    Scene scene;
    SceneNode wall; wall.ifcType = "IfcWall"; scene.AddNode(std::move(wall));
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);
    panel.ApplyDefaults();

    EXPECT_TRUE(panel.IsTypeVisible("IfcWall"));
    // Does not surface missing types.
    EXPECT_EQ(panel.GetTypes().size(), 1u);
}

TEST(TypeVisibilityPanelTest, SetSceneNullClearsState) {
    Scene scene = MakeThreeTypeScene();
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);
    panel.SetScene(nullptr);
    EXPECT_TRUE(panel.GetTypes().empty());
}

TEST(TypeVisibilityPanelTest, RefreshPicksUpNewTypes) {
    Scene scene;
    SceneNode wall; wall.ifcType = "IfcWall"; scene.AddNode(std::move(wall));
    TypeVisibilityPanel panel;
    panel.SetScene(&scene);
    EXPECT_EQ(panel.GetTypes().size(), 1u);

    SceneNode slab; slab.ifcType = "IfcSlab"; scene.AddNode(std::move(slab));
    panel.Refresh();
    EXPECT_EQ(panel.GetTypes().size(), 2u);
}

}  // namespace
