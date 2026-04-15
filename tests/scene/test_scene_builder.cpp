#include <gtest/gtest.h>

#include <scene/SceneBuilder.h>
#include <scene/Scene.h>
#include <scene/SceneMesh.h>
#include <scene/SceneNode.h>
#include <ifc/IfcGeometryExtractor.h>
#include <ifc/IfcModel.h>

#include <string>

static const std::string kTestFile =
    std::string(TEST_DATA_DIR) + "/example.ifc";

class SceneBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(model_.LoadFromFile(kTestFile));
    }

    bimeup::ifc::IfcModel model_;
};

TEST_F(SceneBuilderTest, BuildProducesNonEmptyScene) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    EXPECT_GT(result.scene.GetNodeCount(), 0u);
}

TEST_F(SceneBuilderTest, BuildProducesMeshes) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    EXPECT_GT(result.meshes.size(), 0u);
}

TEST_F(SceneBuilderTest, MeshesHaveValidData) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    for (const auto& mesh : result.meshes) {
        EXPECT_GT(mesh.GetVertexCount(), 0u);
        EXPECT_GT(mesh.GetIndexCount(), 0u);
        EXPECT_EQ(mesh.GetPositions().size(), mesh.GetNormals().size());
        EXPECT_EQ(mesh.GetPositions().size(), mesh.GetColors().size());
    }
}

TEST_F(SceneBuilderTest, NodesWithMeshHaveValidHandle) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    size_t meshNodeCount = 0;
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        const auto& node = result.scene.GetNode(static_cast<uint32_t>(i));
        if (node.mesh.has_value()) {
            EXPECT_LT(node.mesh.value(), result.meshes.size());
            ++meshNodeCount;
        }
    }
    EXPECT_GT(meshNodeCount, 0u);
}

TEST_F(SceneBuilderTest, NodesHaveAABB) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    size_t boundsCount = 0;
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        const auto& node = result.scene.GetNode(static_cast<uint32_t>(i));
        if (node.mesh.has_value() && node.bounds.IsValid()) {
            ++boundsCount;
        }
    }
    EXPECT_GT(boundsCount, 0u);
}

TEST_F(SceneBuilderTest, HierarchyIsPreserved) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    auto roots = result.scene.GetRoots();
    EXPECT_GE(roots.size(), 1u);

    // At least one root should have children (spatial structure)
    bool hasChildren = false;
    for (auto rootId : roots) {
        if (!result.scene.GetChildren(rootId).empty()) {
            hasChildren = true;
            break;
        }
    }
    EXPECT_TRUE(hasChildren);
}

// 7.8b: each mesh-bearing SceneNode represents one IfcPlacedGeometry and sits
// under a shared element parent that carries the IFC expressId but no mesh of
// its own. That lets a single IFC element contribute several sub-meshes (e.g.
// a window frame + translucent glass pane) while still resolving to one
// selectable element.
TEST_F(SceneBuilderTest, MeshBearingNodesHaveElementParentWithSameExpressId) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    size_t checked = 0;
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        auto id = static_cast<bimeup::scene::NodeId>(i);
        const auto& node = result.scene.GetNode(id);
        if (!node.mesh.has_value()) continue;
        if (node.expressId == 0) continue;
        ASSERT_NE(node.parent, bimeup::scene::InvalidNodeId)
            << "mesh-bearing node must have a parent";
        const auto& parent = result.scene.GetNode(node.parent);
        EXPECT_EQ(parent.expressId, node.expressId);
        EXPECT_FALSE(parent.mesh.has_value())
            << "element parent should not carry a mesh itself";
        ++checked;
    }
    EXPECT_GT(checked, 0u);
}

TEST_F(SceneBuilderTest, SubMeshChildCountMatchesExtractSubMeshes) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    bimeup::ifc::IfcGeometryExtractor extractor(model_);

    size_t elementsChecked = 0;
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        auto id = static_cast<bimeup::scene::NodeId>(i);
        const auto& node = result.scene.GetNode(id);
        if (node.expressId == 0) continue;
        if (node.mesh.has_value()) continue;  // only element parents

        size_t meshChildren = 0;
        for (auto childId : result.scene.GetChildren(id)) {
            if (result.scene.GetNode(childId).mesh.has_value() &&
                result.scene.GetNode(childId).expressId == node.expressId) {
                ++meshChildren;
            }
        }
        if (meshChildren == 0) continue;

        auto subs = extractor.ExtractSubMeshes(node.expressId);
        EXPECT_EQ(subs.size(), meshChildren) << "expressId=" << node.expressId;
        ++elementsChecked;
    }
    EXPECT_GT(elementsChecked, 0u);
}

TEST_F(SceneBuilderTest, NodesHaveIfcMetadata) {
    bimeup::scene::SceneBuilder builder;
    auto result = builder.Build(model_);

    bool foundTypedNode = false;
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        const auto& node = result.scene.GetNode(static_cast<uint32_t>(i));
        if (!node.ifcType.empty()) {
            foundTypedNode = true;
            break;
        }
    }
    EXPECT_TRUE(foundTypedNode);
}
