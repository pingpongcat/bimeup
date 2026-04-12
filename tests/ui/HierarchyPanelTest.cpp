#include <gtest/gtest.h>

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

TEST(HierarchyPanelTest, SetRootNullClearsState) {
    HierarchyNode root = MakeNode("IfcProject", "Project");
    HierarchyPanel panel;
    panel.SetRoot(&root);
    panel.SetRoot(nullptr);
    EXPECT_EQ(panel.GetDepth(), 0u);
    EXPECT_EQ(panel.GetNodeCount(), 0u);
}

}  // namespace
