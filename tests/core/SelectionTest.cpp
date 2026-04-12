#include <gtest/gtest.h>

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Selection.h"

namespace bimeup::core {

TEST(SelectionTest, StartsEmpty) {
    EventBus bus;
    Selection selection(bus);

    EXPECT_EQ(selection.Count(), 0u);
    EXPECT_TRUE(selection.Ids().empty());
    EXPECT_FALSE(selection.Contains(42u));
}

TEST(SelectionTest, NonAdditiveClickReplacesSelection) {
    EventBus bus;
    Selection selection(bus);

    bus.Publish(ElementSelected{.expressId = 1, .additive = false});
    EXPECT_EQ(selection.Count(), 1u);
    EXPECT_TRUE(selection.Contains(1u));

    bus.Publish(ElementSelected{.expressId = 2, .additive = false});
    EXPECT_EQ(selection.Count(), 1u);
    EXPECT_FALSE(selection.Contains(1u));
    EXPECT_TRUE(selection.Contains(2u));
}

TEST(SelectionTest, AdditiveClickAddsToSelection) {
    EventBus bus;
    Selection selection(bus);

    bus.Publish(ElementSelected{.expressId = 1, .additive = false});
    bus.Publish(ElementSelected{.expressId = 2, .additive = true});
    bus.Publish(ElementSelected{.expressId = 3, .additive = true});

    EXPECT_EQ(selection.Count(), 3u);
    EXPECT_TRUE(selection.Contains(1u));
    EXPECT_TRUE(selection.Contains(2u));
    EXPECT_TRUE(selection.Contains(3u));
}

TEST(SelectionTest, AdditiveClickOnAlreadySelectedTogglesOff) {
    EventBus bus;
    Selection selection(bus);

    bus.Publish(ElementSelected{.expressId = 1, .additive = false});
    bus.Publish(ElementSelected{.expressId = 2, .additive = true});
    bus.Publish(ElementSelected{.expressId = 1, .additive = true});

    EXPECT_EQ(selection.Count(), 1u);
    EXPECT_FALSE(selection.Contains(1u));
    EXPECT_TRUE(selection.Contains(2u));
}

TEST(SelectionTest, ClearRemovesAll) {
    EventBus bus;
    Selection selection(bus);

    bus.Publish(ElementSelected{.expressId = 1, .additive = false});
    bus.Publish(ElementSelected{.expressId = 2, .additive = true});
    bus.Publish(ElementSelected{.expressId = 3, .additive = true});

    selection.Clear();

    EXPECT_EQ(selection.Count(), 0u);
    EXPECT_FALSE(selection.Contains(1u));
    EXPECT_FALSE(selection.Contains(2u));
    EXPECT_FALSE(selection.Contains(3u));
}

TEST(SelectionTest, ChangeCallbackFiresOnSelectAndClear) {
    EventBus bus;
    Selection selection(bus);

    int changes = 0;
    selection.SetOnChanged([&]() { ++changes; });

    bus.Publish(ElementSelected{.expressId = 1, .additive = false});
    EXPECT_EQ(changes, 1);

    bus.Publish(ElementSelected{.expressId = 2, .additive = true});
    EXPECT_EQ(changes, 2);

    selection.Clear();
    EXPECT_EQ(changes, 3);
}

TEST(SelectionTest, ClearOnEmptyDoesNotFireChange) {
    EventBus bus;
    Selection selection(bus);

    int changes = 0;
    selection.SetOnChanged([&]() { ++changes; });

    selection.Clear();
    EXPECT_EQ(changes, 0);
}

}  // namespace bimeup::core
