#include <gtest/gtest.h>
#include <ifc/IfcHierarchy.h>
#include <ifc/IfcModel.h>

class IfcHierarchyTest : public ::testing::Test {
protected:
    static constexpr const char* kTestFile = TEST_DATA_DIR "/example.ifc";

    bimeup::ifc::IfcModel model;

    void SetUp() override {
        ASSERT_TRUE(model.LoadFromFile(kTestFile));
    }
};

TEST_F(IfcHierarchyTest, Build_FromModel_RootIsProject) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();
    EXPECT_EQ(root.type, "IfcProject");
    EXPECT_GT(root.expressId, 0u);
}

TEST_F(IfcHierarchyTest, Build_FromModel_DepthAtLeastThree) {
    // Project -> Site -> Building -> Storey is depth 4
    bimeup::ifc::IfcHierarchy hierarchy(model);
    EXPECT_GE(hierarchy.GetDepth(), 3u);
}

TEST_F(IfcHierarchyTest, Build_FromModel_HasSiteUnderProject) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();
    ASSERT_FALSE(root.children.empty());

    bool hasSite = false;
    for (const auto& child : root.children) {
        if (child.type == "IfcSite") {
            hasSite = true;
            break;
        }
    }
    EXPECT_TRUE(hasSite);
}

TEST_F(IfcHierarchyTest, Build_FromModel_HasBuildingUnderSite) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();

    bool hasBuilding = false;
    for (const auto& site : root.children) {
        for (const auto& child : site.children) {
            if (child.type == "IfcBuilding") {
                hasBuilding = true;
                break;
            }
        }
    }
    EXPECT_TRUE(hasBuilding);
}

TEST_F(IfcHierarchyTest, Build_FromModel_HasStoreysUnderBuilding) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();

    size_t storeyCount = 0;
    for (const auto& site : root.children) {
        for (const auto& building : site.children) {
            for (const auto& child : building.children) {
                if (child.type == "IfcBuildingStorey") {
                    storeyCount++;
                }
            }
        }
    }
    // The test file has 2 storeys (Level 1 and Level 2)
    EXPECT_GE(storeyCount, 2u);
}

TEST_F(IfcHierarchyTest, Build_FromModel_ElementCountReasonable) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    size_t hierarchyCount = hierarchy.GetElementCount();
    size_t modelCount = model.GetElementCount();
    // Not all elements may be spatially placed, but most should be
    EXPECT_GT(hierarchyCount, 0u);
    EXPECT_LE(hierarchyCount, modelCount);
    // At least 90% of elements should be in the spatial hierarchy
    EXPECT_GE(hierarchyCount, modelCount * 9 / 10);
}

TEST_F(IfcHierarchyTest, FindNode_ExistingId_ReturnsNode) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();
    const auto* found = hierarchy.FindNode(root.expressId);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->expressId, root.expressId);
}

TEST_F(IfcHierarchyTest, FindNode_InvalidId_ReturnsNull) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    EXPECT_EQ(hierarchy.FindNode(999999), nullptr);
}

TEST_F(IfcHierarchyTest, Nodes_HaveGlobalIds) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();
    EXPECT_FALSE(root.globalId.empty());
}

TEST_F(IfcHierarchyTest, StoreyElements_HaveCorrectTypes) {
    bimeup::ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();

    // Navigate to first storey and check its children are IFC element types
    for (const auto& site : root.children) {
        for (const auto& building : site.children) {
            for (const auto& storey : building.children) {
                if (storey.type == "IfcBuildingStorey" && !storey.children.empty()) {
                    // Elements under a storey should have non-empty types
                    EXPECT_FALSE(storey.children[0].type.empty());
                    return;
                }
            }
        }
    }
    FAIL() << "No storey with elements found";
}
