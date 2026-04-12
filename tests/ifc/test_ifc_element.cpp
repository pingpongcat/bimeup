#include <gtest/gtest.h>
#include <ifc/IfcElement.h>
#include <ifc/IfcModel.h>

class IfcElementTest : public ::testing::Test {
protected:
    static constexpr const char* kTestFile = TEST_DATA_DIR "/example.ifc";

    bimeup::ifc::IfcModel model;

    void SetUp() override {
        ASSERT_TRUE(model.LoadFromFile(kTestFile));
    }
};

TEST_F(IfcElementTest, GetElement_HasCorrectType) {
    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());

    auto element = model.GetElement(columns[0]);
    ASSERT_TRUE(element.has_value());
    EXPECT_EQ(element->type, "IfcColumn");
}

TEST_F(IfcElementTest, GetElement_HasNonEmptyGlobalId) {
    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    auto element = model.GetElement(ids[0]);
    ASSERT_TRUE(element.has_value());
    EXPECT_FALSE(element->globalId.empty());
}

TEST_F(IfcElementTest, GetElement_HasExpressId) {
    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    auto element = model.GetElement(ids[0]);
    ASSERT_TRUE(element.has_value());
    EXPECT_EQ(element->expressId, ids[0]);
}

TEST_F(IfcElementTest, GetElement_ColumnHasName) {
    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());

    auto element = model.GetElement(columns[0]);
    ASSERT_TRUE(element.has_value());
    EXPECT_FALSE(element->name.empty());
}

TEST_F(IfcElementTest, GetElement_InvalidExpressId_ReturnsNullopt) {
    auto element = model.GetElement(999999);
    EXPECT_FALSE(element.has_value());
}

TEST_F(IfcElementTest, GetElements_ReturnsAllElements) {
    auto elements = model.GetElements();
    EXPECT_EQ(elements.size(), model.GetElementCount());
    EXPECT_GT(elements.size(), 0u);
}

TEST_F(IfcElementTest, GetElements_AllHaveNonEmptyGlobalId) {
    auto elements = model.GetElements();
    for (const auto& elem : elements) {
        EXPECT_FALSE(elem.globalId.empty()) << "Element express ID " << elem.expressId << " has empty globalId";
    }
}

TEST_F(IfcElementTest, GetElements_AllHaveNonEmptyType) {
    auto elements = model.GetElements();
    for (const auto& elem : elements) {
        EXPECT_FALSE(elem.type.empty()) << "Element express ID " << elem.expressId << " has empty type";
    }
}

TEST_F(IfcElementTest, GetElementsByType_ReturnsCorrectTypes) {
    auto columns = model.GetElementsByType("IFCCOLUMN");
    EXPECT_GT(columns.size(), 0u);
    for (const auto& elem : columns) {
        EXPECT_EQ(elem.type, "IfcColumn");
    }
}

TEST_F(IfcElementTest, GetElementByGlobalId_FindsElement) {
    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    std::string guid = model.GetGlobalId(ids[0]);
    ASSERT_FALSE(guid.empty());

    auto element = model.GetElementByGlobalId(guid);
    ASSERT_TRUE(element.has_value());
    EXPECT_EQ(element->globalId, guid);
    EXPECT_EQ(element->expressId, ids[0]);
}

TEST_F(IfcElementTest, GetElementByGlobalId_UnknownGuid_ReturnsNullopt) {
    auto element = model.GetElementByGlobalId("NONEXISTENT_GUID_12345");
    EXPECT_FALSE(element.has_value());
}
