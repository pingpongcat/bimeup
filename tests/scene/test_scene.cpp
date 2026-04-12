#include <gtest/gtest.h>
#include <scene/Scene.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace bimeup::scene;

TEST(SceneNodeTest, DefaultConstruction) {
    SceneNode node;
    EXPECT_EQ(node.id, InvalidNodeId);
    EXPECT_TRUE(node.name.empty());
    EXPECT_TRUE(node.ifcType.empty());
    EXPECT_TRUE(node.globalId.empty());
    EXPECT_EQ(node.transform, glm::mat4(1.0f));
    EXPECT_FALSE(node.mesh.has_value());
    EXPECT_EQ(node.parent, InvalidNodeId);
    EXPECT_TRUE(node.children.empty());
    EXPECT_TRUE(node.visible);
    EXPECT_FALSE(node.selected);
}

TEST(SceneTest, AddNodeReturnsValidId) {
    Scene scene;
    SceneNode node;
    node.name = "Wall1";
    node.ifcType = "IfcWall";
    node.globalId = "abc-123";

    NodeId id = scene.AddNode(std::move(node));
    EXPECT_NE(id, InvalidNodeId);
    EXPECT_EQ(scene.GetNodeCount(), 1);
}

TEST(SceneTest, GetNodeById) {
    Scene scene;
    SceneNode node;
    node.name = "Slab1";
    node.ifcType = "IfcSlab";
    node.globalId = "def-456";

    NodeId id = scene.AddNode(std::move(node));
    const auto& retrieved = scene.GetNode(id);
    EXPECT_EQ(retrieved.id, id);
    EXPECT_EQ(retrieved.name, "Slab1");
    EXPECT_EQ(retrieved.ifcType, "IfcSlab");
    EXPECT_EQ(retrieved.globalId, "def-456");
}

TEST(SceneTest, ParentChildRelations) {
    Scene scene;

    SceneNode parent;
    parent.name = "Building";
    NodeId parentId = scene.AddNode(std::move(parent));

    SceneNode child;
    child.name = "Floor1";
    child.parent = parentId;
    NodeId childId = scene.AddNode(std::move(child));

    // Child should reference parent
    EXPECT_EQ(scene.GetNode(childId).parent, parentId);
    // Parent should list child
    auto children = scene.GetChildren(parentId);
    ASSERT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], childId);
}

TEST(SceneTest, GetRootsReturnsOnlyRootNodes) {
    Scene scene;

    SceneNode root1;
    root1.name = "Site";
    NodeId root1Id = scene.AddNode(std::move(root1));

    SceneNode root2;
    root2.name = "Site2";
    NodeId root2Id = scene.AddNode(std::move(root2));

    SceneNode child;
    child.name = "Building";
    child.parent = root1Id;
    scene.AddNode(std::move(child));

    auto roots = scene.GetRoots();
    ASSERT_EQ(roots.size(), 2);
    EXPECT_EQ(roots[0], root1Id);
    EXPECT_EQ(roots[1], root2Id);
}

TEST(SceneTest, SetVisibilityNonRecursive) {
    Scene scene;

    SceneNode parent;
    parent.name = "Building";
    NodeId parentId = scene.AddNode(std::move(parent));

    SceneNode child;
    child.name = "Floor";
    child.parent = parentId;
    NodeId childId = scene.AddNode(std::move(child));

    scene.SetVisibility(parentId, false);
    EXPECT_FALSE(scene.GetNode(parentId).visible);
    EXPECT_TRUE(scene.GetNode(childId).visible); // child unaffected
}

TEST(SceneTest, SetVisibilityRecursive) {
    Scene scene;

    SceneNode parent;
    parent.name = "Building";
    NodeId parentId = scene.AddNode(std::move(parent));

    SceneNode child;
    child.name = "Floor";
    child.parent = parentId;
    NodeId childId = scene.AddNode(std::move(child));

    SceneNode grandchild;
    grandchild.name = "Wall";
    grandchild.parent = childId;
    NodeId grandchildId = scene.AddNode(std::move(grandchild));

    scene.SetVisibility(parentId, false, true);
    EXPECT_FALSE(scene.GetNode(parentId).visible);
    EXPECT_FALSE(scene.GetNode(childId).visible);
    EXPECT_FALSE(scene.GetNode(grandchildId).visible);
}

TEST(SceneTest, SetSelectedAndGetSelected) {
    Scene scene;

    SceneNode n1;
    n1.name = "Wall1";
    NodeId id1 = scene.AddNode(std::move(n1));

    SceneNode n2;
    n2.name = "Wall2";
    NodeId id2 = scene.AddNode(std::move(n2));

    SceneNode n3;
    n3.name = "Wall3";
    scene.AddNode(std::move(n3));

    scene.SetSelected(id1, true);
    scene.SetSelected(id2, true);

    auto selected = scene.GetSelected();
    ASSERT_EQ(selected.size(), 2);
    EXPECT_EQ(selected[0], id1);
    EXPECT_EQ(selected[1], id2);

    scene.SetSelected(id1, false);
    selected = scene.GetSelected();
    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected[0], id2);
}

TEST(SceneTest, FindByType) {
    Scene scene;

    SceneNode wall;
    wall.ifcType = "IfcWall";
    scene.AddNode(std::move(wall));

    SceneNode slab;
    slab.ifcType = "IfcSlab";
    scene.AddNode(std::move(slab));

    SceneNode wall2;
    wall2.ifcType = "IfcWall";
    scene.AddNode(std::move(wall2));

    auto walls = scene.FindByType("IfcWall");
    EXPECT_EQ(walls.size(), 2);

    auto slabs = scene.FindByType("IfcSlab");
    EXPECT_EQ(slabs.size(), 1);

    auto doors = scene.FindByType("IfcDoor");
    EXPECT_EQ(doors.size(), 0);
}

TEST(SceneTest, MultipleChildrenOrdering) {
    Scene scene;

    SceneNode parent;
    parent.name = "Storey";
    NodeId parentId = scene.AddNode(std::move(parent));

    SceneNode c1;
    c1.name = "Wall1";
    c1.parent = parentId;
    NodeId c1Id = scene.AddNode(std::move(c1));

    SceneNode c2;
    c2.name = "Wall2";
    c2.parent = parentId;
    NodeId c2Id = scene.AddNode(std::move(c2));

    SceneNode c3;
    c3.name = "Wall3";
    c3.parent = parentId;
    NodeId c3Id = scene.AddNode(std::move(c3));

    auto children = scene.GetChildren(parentId);
    ASSERT_EQ(children.size(), 3);
    EXPECT_EQ(children[0], c1Id);
    EXPECT_EQ(children[1], c2Id);
    EXPECT_EQ(children[2], c3Id);
}

TEST(SceneTest, GetNodeThrowsForInvalidId) {
    Scene scene;
    EXPECT_THROW(scene.GetNode(42), std::out_of_range);
}

TEST(SceneTest, NodeTransformIsPreserved) {
    Scene scene;
    SceneNode node;
    node.name = "Rotated";
    node.transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));

    NodeId id = scene.AddNode(std::move(node));
    const auto& n = scene.GetNode(id);
    EXPECT_EQ(n.transform[3][0], 1.0f);
    EXPECT_EQ(n.transform[3][1], 2.0f);
    EXPECT_EQ(n.transform[3][2], 3.0f);
}
