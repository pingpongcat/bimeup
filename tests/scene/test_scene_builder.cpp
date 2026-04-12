#include <gtest/gtest.h>

#include <scene/SceneBuilder.h>
#include <scene/Scene.h>
#include <scene/SceneMesh.h>
#include <scene/SceneNode.h>
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
