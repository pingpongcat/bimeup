#include <gtest/gtest.h>

#include <core/EventBus.h>
#include <core/Events.h>
#include <ifc/IfcHierarchy.h>
#include <ui/HierarchyPanel.h>

namespace {

using bimeup::ifc::HierarchyNode;
using bimeup::ui::HierarchyPanel;

HierarchyNode MakeNode(const std::string& type, const std::string& name) {
    HierarchyNode node;
    node.type = type;
    node.name = name;
    return node;
}

TEST(HierarchyPanelTest, DefaultIsEmpty) {
    HierarchyPanel panel;
    EXPECT_EQ(panel.GetRoot(), nullptr);
    EXPECT_EQ(panel.GetDepth(), 0u);
    EXPECT_EQ(panel.GetNodeCount(), 0u);
}

TEST(HierarchyPanelTest, HasPanelName) {
    HierarchyPanel panel;
    EXPECT_STREQ(panel.GetName(), "Hierarchy");
}

TEST(HierarchyPanelTest, DepthOfSingleRoot) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    HierarchyPanel panel;
    panel.SetRoot(&root);
    EXPECT_EQ(panel.GetDepth(), 1u);
    EXPECT_EQ(panel.GetNodeCount(), 1u);
}

TEST(HierarchyPanelTest, DepthOfLinearChain) {
    HierarchyNode site = MakeNode("IfcSite", "Site");
    HierarchyNode building = MakeNode("IfcBuilding", "Building");
    HierarchyNode storey = MakeNode("IfcBuildingStorey", "L1");
    building.children.push_back(storey);
    site.children.push_back(building);

    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.children.push_back(site);

    HierarchyPanel panel;
    panel.SetRoot(&root);
    EXPECT_EQ(panel.GetDepth(), 4u);
    EXPECT_EQ(panel.GetNodeCount(), 4u);
}

TEST(HierarchyPanelTest, DepthOfBranchingTree) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    HierarchyNode siteA = MakeNode("IfcSite", "A");
    HierarchyNode siteB = MakeNode("IfcSite", "B");
    HierarchyNode storey = MakeNode("IfcBuildingStorey", "L1");
    siteB.children.push_back(storey);
    root.children.push_back(siteA);
    root.children.push_back(siteB);

    HierarchyPanel panel;
    panel.SetRoot(&root);
    EXPECT_EQ(panel.GetDepth(), 3u);
    EXPECT_EQ(panel.GetNodeCount(), 4u);
}

TEST(HierarchyPanelTest, ElementSelectedMarksRowAsSelected) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode wall = MakeNode("IfcWall", "W");
    wall.expressId = 42;
    root.children.push_back(wall);

    bimeup::core::EventBus bus;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetEventBus(&bus);

    EXPECT_FALSE(panel.IsSelected(42));
    bus.Publish(bimeup::core::ElementSelected{42, false});
    EXPECT_TRUE(panel.IsSelected(42));
    EXPECT_FALSE(panel.IsSelected(1));
}

TEST(HierarchyPanelTest, ElementSelectedMarksAncestorsForExpand) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode site = MakeNode("IfcSite", "Site");
    site.expressId = 2;
    HierarchyNode wall = MakeNode("IfcWall", "W");
    wall.expressId = 42;
    site.children.push_back(wall);
    root.children.push_back(site);

    bimeup::core::EventBus bus;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetEventBus(&bus);

    bus.Publish(bimeup::core::ElementSelected{42, false});
    EXPECT_TRUE(panel.IsAncestorOfSelection(1));
    EXPECT_TRUE(panel.IsAncestorOfSelection(2));
    EXPECT_FALSE(panel.IsAncestorOfSelection(42));  // the selected node itself is not its own ancestor
}

TEST(HierarchyPanelTest, NonAdditiveElementSelectedReplacesPrevious) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode a = MakeNode("IfcWall", "A"); a.expressId = 10;
    HierarchyNode b = MakeNode("IfcWall", "B"); b.expressId = 20;
    root.children.push_back(a);
    root.children.push_back(b);

    bimeup::core::EventBus bus;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetEventBus(&bus);

    bus.Publish(bimeup::core::ElementSelected{10, false});
    bus.Publish(bimeup::core::ElementSelected{20, false});
    EXPECT_FALSE(panel.IsSelected(10));
    EXPECT_TRUE(panel.IsSelected(20));
}

TEST(HierarchyPanelTest, AdditiveElementSelectedAccumulates) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode a = MakeNode("IfcWall", "A"); a.expressId = 10;
    HierarchyNode b = MakeNode("IfcWall", "B"); b.expressId = 20;
    root.children.push_back(a);
    root.children.push_back(b);

    bimeup::core::EventBus bus;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetEventBus(&bus);

    bus.Publish(bimeup::core::ElementSelected{10, false});
    bus.Publish(bimeup::core::ElementSelected{20, true});  // additive
    EXPECT_TRUE(panel.IsSelected(10));
    EXPECT_TRUE(panel.IsSelected(20));
}

TEST(HierarchyPanelTest, SelectionClearedResetsState) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode wall = MakeNode("IfcWall", "W");
    wall.expressId = 42;
    root.children.push_back(wall);

    bimeup::core::EventBus bus;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetEventBus(&bus);

    bus.Publish(bimeup::core::ElementSelected{42, false});
    bus.Publish(bimeup::core::SelectionCleared{});
    EXPECT_FALSE(panel.IsSelected(42));
    EXPECT_FALSE(panel.IsAncestorOfSelection(1));
}

TEST(HierarchyPanelTest, TriggerToggleVisibilityFiresCallbackWithNode) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode wall = MakeNode("IfcWall", "W");
    wall.expressId = 42;
    root.children.push_back(wall);

    HierarchyPanel panel;
    panel.SetRoot(&root);

    std::uint32_t captured = 0;
    panel.SetOnToggleVisibility(
        [&](const HierarchyNode& n) { captured = n.expressId; });

    panel.TriggerToggleVisibility(root.children[0]);
    EXPECT_EQ(captured, 42u);
}

TEST(HierarchyPanelTest, TriggerIsolateFiresCallbackWithNode) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyNode wall = MakeNode("IfcWall", "W");
    wall.expressId = 42;
    root.children.push_back(wall);

    HierarchyPanel panel;
    panel.SetRoot(&root);

    std::uint32_t captured = 0;
    panel.SetOnIsolate([&](const HierarchyNode& n) { captured = n.expressId; });

    panel.TriggerIsolate(root.children[0]);
    EXPECT_EQ(captured, 42u);
}

TEST(HierarchyPanelTest, TriggersAreNoOpWithoutCallback) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    root.expressId = 1;
    HierarchyPanel panel;
    panel.SetRoot(&root);
    // Must not crash when no callback wired.
    panel.TriggerToggleVisibility(root);
    panel.TriggerIsolate(root);
    SUCCEED();
}

TEST(HierarchyPanelTest, SetRootNullClearsState) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetRoot(nullptr);
    EXPECT_EQ(panel.GetDepth(), 0u);
    EXPECT_EQ(panel.GetNodeCount(), 0u);
}

}  // namespace
