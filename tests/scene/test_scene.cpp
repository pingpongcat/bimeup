#include <gtest/gtest.h>
#include <scene/Scene.h>

#include <algorithm>

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

TEST(SceneTest, SetVisibilityByTypeHidesOnlyMatchingNodes) {
    Scene scene;

    SceneNode wall1;
    wall1.ifcType = "IfcWall";
    NodeId wall1Id = scene.AddNode(std::move(wall1));

    SceneNode slab;
    slab.ifcType = "IfcSlab";
    NodeId slabId = scene.AddNode(std::move(slab));

    SceneNode wall2;
    wall2.ifcType = "IfcWall";
    NodeId wall2Id = scene.AddNode(std::move(wall2));

    size_t affected = scene.SetVisibilityByType("IfcWall", false);
    EXPECT_EQ(affected, 2u);
    EXPECT_FALSE(scene.GetNode(wall1Id).visible);
    EXPECT_FALSE(scene.GetNode(wall2Id).visible);
    EXPECT_TRUE(scene.GetNode(slabId).visible);

    affected = scene.SetVisibilityByType("IfcWall", true);
    EXPECT_EQ(affected, 2u);
    EXPECT_TRUE(scene.GetNode(wall1Id).visible);
    EXPECT_TRUE(scene.GetNode(wall2Id).visible);
}

TEST(SceneTest, SetVisibilityByTypeUnknownTypeAffectsNothing) {
    Scene scene;
    SceneNode wall;
    wall.ifcType = "IfcWall";
    NodeId wallId = scene.AddNode(std::move(wall));

    size_t affected = scene.SetVisibilityByType("IfcDoor", false);
    EXPECT_EQ(affected, 0u);
    EXPECT_TRUE(scene.GetNode(wallId).visible);
}

TEST(SceneTest, GetUniqueTypesReturnsSortedUniqueList) {
    Scene scene;

    SceneNode a; a.ifcType = "IfcWall"; scene.AddNode(std::move(a));
    SceneNode b; b.ifcType = "IfcSlab"; scene.AddNode(std::move(b));
    SceneNode c; c.ifcType = "IfcWall"; scene.AddNode(std::move(c));
    SceneNode d; d.ifcType = "IfcDoor"; scene.AddNode(std::move(d));

    auto types = scene.GetUniqueTypes();
    ASSERT_EQ(types.size(), 3u);
    EXPECT_EQ(types[0], "IfcDoor");
    EXPECT_EQ(types[1], "IfcSlab");
    EXPECT_EQ(types[2], "IfcWall");
}

TEST(SceneTest, GetUniqueTypesSkipsEmptyType) {
    Scene scene;
    SceneNode a; scene.AddNode(std::move(a));  // empty ifcType
    SceneNode b; b.ifcType = "IfcWall"; scene.AddNode(std::move(b));

    auto types = scene.GetUniqueTypes();
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], "IfcWall");
}

TEST(SceneTest, IsolateByExpressIdHidesNonMatchingMeshNodes) {
    Scene scene;

    SceneNode wall1;
    wall1.ifcType = "IfcWall";
    wall1.expressId = 101;
    wall1.mesh = MeshHandle{0};
    NodeId wall1Id = scene.AddNode(std::move(wall1));

    SceneNode wall2;
    wall2.ifcType = "IfcWall";
    wall2.expressId = 102;
    wall2.mesh = MeshHandle{0};
    NodeId wall2Id = scene.AddNode(std::move(wall2));

    SceneNode slab;
    slab.ifcType = "IfcSlab";
    slab.expressId = 103;
    slab.mesh = MeshHandle{0};
    NodeId slabId = scene.AddNode(std::move(slab));

    size_t changed = scene.IsolateByExpressId({101, 103});
    EXPECT_EQ(changed, 1u);  // only wall2 flipped
    EXPECT_TRUE(scene.GetNode(wall1Id).visible);
    EXPECT_FALSE(scene.GetNode(wall2Id).visible);
    EXPECT_TRUE(scene.GetNode(slabId).visible);
}

TEST(SceneTest, IsolateByExpressIdReshowsPreviouslyHidden) {
    Scene scene;
    SceneNode wall;
    wall.expressId = 101;
    wall.mesh = MeshHandle{0};
    wall.visible = false;  // type-hidden before
    NodeId wallId = scene.AddNode(std::move(wall));

    size_t changed = scene.IsolateByExpressId({101});
    EXPECT_EQ(changed, 1u);
    EXPECT_TRUE(scene.GetNode(wallId).visible);
}

TEST(SceneTest, IsolateByExpressIdLeavesNonMeshNodesUntouched) {
    Scene scene;

    SceneNode storey;  // no mesh, no expressId match
    storey.ifcType = "IfcBuildingStorey";
    NodeId storeyId = scene.AddNode(std::move(storey));

    SceneNode wall;
    wall.expressId = 101;
    wall.parent = storeyId;
    wall.mesh = MeshHandle{0};
    NodeId wallId = scene.AddNode(std::move(wall));

    scene.IsolateByExpressId({999});  // nothing matches
    EXPECT_TRUE(scene.GetNode(storeyId).visible);   // ancestor preserved
    EXPECT_FALSE(scene.GetNode(wallId).visible);     // hidden
}

TEST(SceneTest, IsolateByExpressIdEmptySetHidesAllMeshes) {
    Scene scene;
    SceneNode a; a.expressId = 1; a.mesh = MeshHandle{0};
    NodeId aId = scene.AddNode(std::move(a));
    SceneNode b; b.expressId = 2; b.mesh = MeshHandle{0};
    NodeId bId = scene.AddNode(std::move(b));

    size_t changed = scene.IsolateByExpressId({});
    EXPECT_EQ(changed, 2u);
    EXPECT_FALSE(scene.GetNode(aId).visible);
    EXPECT_FALSE(scene.GetNode(bId).visible);
}

TEST(SceneTest, ShowAllRestoresVisibility) {
    Scene scene;
    SceneNode a; a.expressId = 1; a.mesh = MeshHandle{0};
    NodeId aId = scene.AddNode(std::move(a));
    SceneNode b; b.expressId = 2; b.mesh = MeshHandle{0}; b.visible = false;
    NodeId bId = scene.AddNode(std::move(b));

    scene.ShowAll();
    EXPECT_TRUE(scene.GetNode(aId).visible);
    EXPECT_TRUE(scene.GetNode(bId).visible);
}

TEST(SceneTest, DefaultHiddenTypesContainsNonVisualTypes) {
    const auto& defaults = DefaultHiddenTypes();
    auto has = [&](const std::string& s) {
        return std::find(defaults.begin(), defaults.end(), s) != defaults.end();
    };
    EXPECT_TRUE(has("IfcSpace"));
    EXPECT_TRUE(has("IfcOpeningElement"));
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

// ----- 7.8d Alpha overrides -----------------------------------------------

namespace {
NodeId AddMeshNode(Scene& scene, std::uint32_t expressId, const std::string& ifcType) {
    SceneNode n;
    n.expressId = expressId;
    n.ifcType = ifcType;
    n.mesh = MeshHandle{0};
    return scene.AddNode(std::move(n));
}
}  // namespace

TEST(SceneAlphaOverrideTest, ElementOverrideRoundTrip) {
    Scene scene;
    NodeId id = AddMeshNode(scene, 101, "IfcWall");

    EXPECT_FALSE(scene.GetElementAlphaOverride(101).has_value());
    EXPECT_FALSE(scene.GetEffectiveAlpha(id).has_value());

    scene.SetElementAlphaOverride(101, 0.3f);
    ASSERT_TRUE(scene.GetElementAlphaOverride(101).has_value());
    EXPECT_FLOAT_EQ(*scene.GetElementAlphaOverride(101), 0.3f);
    ASSERT_TRUE(scene.GetEffectiveAlpha(id).has_value());
    EXPECT_FLOAT_EQ(*scene.GetEffectiveAlpha(id), 0.3f);
}

TEST(SceneAlphaOverrideTest, ElementOverrideClampedToUnitRange) {
    Scene scene;
    AddMeshNode(scene, 7, "IfcSlab");

    scene.SetElementAlphaOverride(7, -1.5f);
    EXPECT_FLOAT_EQ(*scene.GetElementAlphaOverride(7), 0.0f);

    scene.SetElementAlphaOverride(7, 42.0f);
    EXPECT_FLOAT_EQ(*scene.GetElementAlphaOverride(7), 1.0f);
}

TEST(SceneAlphaOverrideTest, ClearElementOverride) {
    Scene scene;
    AddMeshNode(scene, 9, "IfcWall");

    scene.SetElementAlphaOverride(9, 0.25f);
    scene.ClearElementAlphaOverride(9);
    EXPECT_FALSE(scene.GetElementAlphaOverride(9).has_value());
}

TEST(SceneAlphaOverrideTest, TypeOverrideAppliesToAllMatchingNodes) {
    Scene scene;
    NodeId wall1 = AddMeshNode(scene, 1, "IfcWall");
    NodeId wall2 = AddMeshNode(scene, 2, "IfcWall");
    NodeId slab  = AddMeshNode(scene, 3, "IfcSlab");

    scene.SetTypeAlphaOverride("IfcWall", 0.5f);
    ASSERT_TRUE(scene.GetTypeAlphaOverride("IfcWall").has_value());
    EXPECT_FLOAT_EQ(*scene.GetTypeAlphaOverride("IfcWall"), 0.5f);
    EXPECT_FALSE(scene.GetTypeAlphaOverride("IfcSlab").has_value());

    ASSERT_TRUE(scene.GetEffectiveAlpha(wall1).has_value());
    EXPECT_FLOAT_EQ(*scene.GetEffectiveAlpha(wall1), 0.5f);
    EXPECT_FLOAT_EQ(*scene.GetEffectiveAlpha(wall2), 0.5f);
    EXPECT_FALSE(scene.GetEffectiveAlpha(slab).has_value());
}

TEST(SceneAlphaOverrideTest, ElementOverrideWinsOverTypeOverride) {
    Scene scene;
    NodeId id = AddMeshNode(scene, 11, "IfcWall");

    scene.SetTypeAlphaOverride("IfcWall", 0.5f);
    scene.SetElementAlphaOverride(11, 0.1f);

    ASSERT_TRUE(scene.GetEffectiveAlpha(id).has_value());
    EXPECT_FLOAT_EQ(*scene.GetEffectiveAlpha(id), 0.1f);
}

TEST(SceneAlphaOverrideTest, ClearTypeOverrideFallsBackToElementThenNone) {
    Scene scene;
    NodeId id = AddMeshNode(scene, 12, "IfcWall");

    scene.SetTypeAlphaOverride("IfcWall", 0.6f);
    scene.ClearTypeAlphaOverride("IfcWall");
    EXPECT_FALSE(scene.GetTypeAlphaOverride("IfcWall").has_value());
    EXPECT_FALSE(scene.GetEffectiveAlpha(id).has_value());

    scene.SetElementAlphaOverride(12, 0.2f);
    scene.SetTypeAlphaOverride("IfcWall", 0.6f);
    scene.ClearTypeAlphaOverride("IfcWall");
    ASSERT_TRUE(scene.GetEffectiveAlpha(id).has_value());
    EXPECT_FLOAT_EQ(*scene.GetEffectiveAlpha(id), 0.2f);
}

TEST(SceneAlphaOverrideTest, TypeOverrideClampedToUnitRange) {
    Scene scene;
    scene.SetTypeAlphaOverride("IfcWall", 2.5f);
    EXPECT_FLOAT_EQ(*scene.GetTypeAlphaOverride("IfcWall"), 1.0f);
    scene.SetTypeAlphaOverride("IfcWall", -0.1f);
    EXPECT_FLOAT_EQ(*scene.GetTypeAlphaOverride("IfcWall"), 0.0f);
}
