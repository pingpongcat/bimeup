#include <gtest/gtest.h>

#include <ifc/IfcElement.h>
#include <ui/PropertyPanel.h>

namespace {

using bimeup::ifc::IfcElement;
using bimeup::ui::PropertyPanel;

IfcElement MakeElement(uint32_t id, const std::string& type,
                       const std::string& name, const std::string& guid) {
    IfcElement e;
    e.expressId = id;
    e.type = type;
    e.name = name;
    e.globalId = guid;
    return e;
}

TEST(PropertyPanelTest, DefaultIsEmpty) {
    PropertyPanel panel;
    EXPECT_EQ(panel.GetElement(), nullptr);
    EXPECT_FALSE(panel.HasElement());
    EXPECT_EQ(panel.GetPropertyCount(), 0u);
}

TEST(PropertyPanelTest, HasPanelName) {
    PropertyPanel panel;
    EXPECT_STREQ(panel.GetName(), "Properties");
}

TEST(PropertyPanelTest, DisplaysAllFieldsAsKeyValuePairs) {
    const IfcElement elem = MakeElement(42, "IfcWall", "Wall-1", "0abc");
    PropertyPanel panel;
    panel.SetElement(&elem);

    EXPECT_TRUE(panel.HasElement());
    EXPECT_EQ(panel.GetPropertyCount(), 4u);

    EXPECT_EQ(panel.GetPropertyKey(0), "ExpressId");
    EXPECT_EQ(panel.GetPropertyValue(0), "42");

    // Remaining three keys are Type, Name, GlobalId in some order.
    bool foundType = false;
    bool foundName = false;
    bool foundGuid = false;
    for (std::size_t i = 1; i < panel.GetPropertyCount(); ++i) {
        const auto key = panel.GetPropertyKey(i);
        const auto value = panel.GetPropertyValue(i);
        if (key == "Type") {
            foundType = true;
            EXPECT_EQ(value, "IfcWall");
        } else if (key == "Name") {
            foundName = true;
            EXPECT_EQ(value, "Wall-1");
        } else if (key == "GlobalId") {
            foundGuid = true;
            EXPECT_EQ(value, "0abc");
        }
    }
    EXPECT_TRUE(foundType);
    EXPECT_TRUE(foundName);
    EXPECT_TRUE(foundGuid);
}

TEST(PropertyPanelTest, OmitsEmptyOptionalFields) {
    const IfcElement elem = MakeElement(10, "IfcSlab", "", "");
    PropertyPanel panel;
    panel.SetElement(&elem);

    EXPECT_EQ(panel.GetPropertyCount(), 2u);
    EXPECT_EQ(panel.GetPropertyKey(0), "ExpressId");
    EXPECT_EQ(panel.GetPropertyValue(0), "10");
    EXPECT_EQ(panel.GetPropertyKey(1), "Type");
    EXPECT_EQ(panel.GetPropertyValue(1), "IfcSlab");
}

TEST(PropertyPanelTest, SetElementNullClearsState) {
    const IfcElement elem = MakeElement(1, "IfcWall", "W", "G");
    PropertyPanel panel;
    panel.SetElement(&elem);
    panel.SetElement(nullptr);

    EXPECT_FALSE(panel.HasElement());
    EXPECT_EQ(panel.GetElement(), nullptr);
    EXPECT_EQ(panel.GetPropertyCount(), 0u);
}

TEST(PropertyPanelTest, ConstructorWithElement) {
    const IfcElement elem = MakeElement(7, "IfcColumn", "C1", "guid7");
    PropertyPanel panel(&elem);
    EXPECT_TRUE(panel.HasElement());
    EXPECT_EQ(panel.GetElement(), &elem);
    EXPECT_EQ(panel.GetPropertyCount(), 4u);
}

}  // namespace
