#include <gtest/gtest.h>

#include <core/EventBus.h>
#include <core/Events.h>
#include <ifc/IfcElement.h>
#include <ui/HierarchyPanel.h>
#include <ui/PropertyPanel.h>
#include <ui/SelectionBridge.h>

#include <optional>

namespace {

using bimeup::core::ElementSelected;
using bimeup::core::EventBus;
using bimeup::ifc::IfcElement;
using bimeup::ui::HierarchyPanel;
using bimeup::ui::PropertyPanel;
using bimeup::ui::SelectionBridge;

auto EmptyLookup() {
    return [](uint32_t) -> std::optional<IfcElement> { return std::nullopt; };
}

TEST(SelectionBridgeTest, SelectPublishesElementSelected) {
    EventBus bus;
    PropertyPanel panel;
    SelectionBridge bridge(bus, panel, EmptyLookup());

    uint32_t received = 0;
    bool receivedAdditive = true;
    bus.Subscribe<ElementSelected>([&](const ElementSelected& e) {
        received = e.expressId;
        receivedAdditive = e.additive;
    });

    bridge.Select(42);
    EXPECT_EQ(received, 42u);
    EXPECT_FALSE(receivedAdditive);
}

TEST(SelectionBridgeTest, SelectAdditiveForwardsFlag) {
    EventBus bus;
    PropertyPanel panel;
    SelectionBridge bridge(bus, panel, EmptyLookup());

    bool additive = false;
    bus.Subscribe<ElementSelected>([&](const ElementSelected& e) {
        additive = e.additive;
    });

    bridge.Select(1, true);
    EXPECT_TRUE(additive);
}

TEST(SelectionBridgeTest, PopulatesPropertyPanelOnElementSelected) {
    EventBus bus;
    PropertyPanel panel;
    IfcElement stored{42, "GUID-42", "IfcWall", "Wall-01"};
    SelectionBridge bridge(
        bus, panel, [&](uint32_t id) -> std::optional<IfcElement> {
            if (id == 42) return stored;
            return std::nullopt;
        });

    bus.Publish(ElementSelected{42, false});

    ASSERT_TRUE(panel.HasElement());
    EXPECT_EQ(panel.GetElement()->expressId, 42u);
    EXPECT_EQ(panel.GetElement()->name, "Wall-01");
    EXPECT_EQ(panel.GetElement()->type, "IfcWall");
}

TEST(SelectionBridgeTest, ClearsPropertyPanelWhenLookupMisses) {
    EventBus bus;
    PropertyPanel panel;
    IfcElement stored{1, "G", "T", "N"};
    panel.SetElement(&stored);
    SelectionBridge bridge(bus, panel, EmptyLookup());

    bus.Publish(ElementSelected{99, false});

    EXPECT_FALSE(panel.HasElement());
}

TEST(SelectionBridgeTest, HierarchyPanelSelectRoutesThroughBus) {
    EventBus bus;
    PropertyPanel panel;
    IfcElement stored{7, "G-7", "IfcDoor", "D"};
    SelectionBridge bridge(
        bus, panel, [&](uint32_t id) -> std::optional<IfcElement> {
            if (id == 7) return stored;
            return std::nullopt;
        });

    HierarchyPanel hierarchy;
    hierarchy.SetEventBus(&bus);
    hierarchy.Select(7);

    ASSERT_TRUE(panel.HasElement());
    EXPECT_EQ(panel.GetElement()->expressId, 7u);
    EXPECT_EQ(panel.GetElement()->type, "IfcDoor");
}

TEST(SelectionBridgeTest, UnsubscribesOnDestruction) {
    EventBus bus;
    PropertyPanel panel;
    {
        SelectionBridge bridge(bus, panel, EmptyLookup());
    }
    // Should not crash — bridge must unsubscribe before being destroyed.
    bus.Publish(ElementSelected{1, false});
    SUCCEED();
}

}  // namespace
