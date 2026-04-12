#include <gtest/gtest.h>
#include <ifc/IfcModel.h>

class IfcModelTest : public ::testing::Test {
protected:
    static constexpr const char* kTestFile = TEST_DATA_DIR "/example.ifc";
};

TEST_F(IfcModelTest, LoadFromFile_ValidFile_ReturnsTrue) {
    bimeup::ifc::IfcModel model;
    EXPECT_TRUE(model.LoadFromFile(kTestFile));
}

TEST_F(IfcModelTest, LoadFromFile_InvalidPath_ReturnsFalse) {
    bimeup::ifc::IfcModel model;
    EXPECT_FALSE(model.LoadFromFile("/nonexistent/path.ifc"));
}

TEST_F(IfcModelTest, GetElementCount_AfterLoad_GreaterThanZero) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    EXPECT_GT(model.GetElementCount(), 0u);
}

TEST_F(IfcModelTest, GetElementCount_BeforeLoad_ReturnsZero) {
    bimeup::ifc::IfcModel model;
    EXPECT_EQ(model.GetElementCount(), 0u);
}

TEST_F(IfcModelTest, GetElementExpressIds_AfterLoad_NotEmpty) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    auto ids = model.GetElementExpressIds();
    EXPECT_FALSE(ids.empty());
    EXPECT_EQ(ids.size(), model.GetElementCount());
}

TEST_F(IfcModelTest, GetElementsByType_IfcColumn_ReturnsMultiple) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    // The test file has multiple IFCCOLUMN entities
    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    EXPECT_GT(columns.size(), 0u);
}

TEST_F(IfcModelTest, GetElementsByType_UnknownType_ReturnsEmpty) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    auto result = model.GetExpressIdsByType("IFCFAKEENTITY");
    EXPECT_TRUE(result.empty());
}

TEST_F(IfcModelTest, GetElementType_ReturnsCorrectTypeName) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());
    std::string typeName = model.GetElementType(columns[0]);
    // web-ifc returns mixed-case type names (e.g., "IfcColumn")
    EXPECT_EQ(typeName, "IfcColumn");
}

TEST_F(IfcModelTest, GetGlobalId_ReturnsNonEmptyString) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());
    std::string globalId = model.GetGlobalId(ids[0]);
    EXPECT_FALSE(globalId.empty());
}

TEST_F(IfcModelTest, GetElementName_ReturnsString) {
    bimeup::ifc::IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());
    // Column elements in the test file have names
    std::string name = model.GetElementName(columns[0]);
    EXPECT_FALSE(name.empty());
}

TEST_F(IfcModelTest, IsLoaded_BeforeAndAfterLoad) {
    bimeup::ifc::IfcModel model;
    EXPECT_FALSE(model.IsLoaded());
    ASSERT_TRUE(model.LoadFromFile(kTestFile));
    EXPECT_TRUE(model.IsLoaded());
}
