#include <gtest/gtest.h>
#include <core/EventBus.h>

#include <string>

using bimeup::core::EventBus;

namespace {
struct IntEvent {
    int value;
};
struct StringEvent {
    std::string text;
};
}  // namespace

TEST(EventBusTest, PublishWithNoSubscribersIsNoop) {
    EventBus bus;
    bus.Publish(IntEvent{42});
}

TEST(EventBusTest, SubscribeReceivesPublishedEvent) {
    EventBus bus;
    int received = 0;
    bus.Subscribe<IntEvent>([&](const IntEvent& e) { received = e.value; });

    bus.Publish(IntEvent{7});

    EXPECT_EQ(received, 7);
}

TEST(EventBusTest, MultipleSubscribersAllReceiveEvent) {
    EventBus bus;
    int a = 0;
    int b = 0;
    bus.Subscribe<IntEvent>([&](const IntEvent& e) { a = e.value; });
    bus.Subscribe<IntEvent>([&](const IntEvent& e) { b = e.value + 1; });

    bus.Publish(IntEvent{10});

    EXPECT_EQ(a, 10);
    EXPECT_EQ(b, 11);
}

TEST(EventBusTest, DifferentEventTypesAreIsolated) {
    EventBus bus;
    int intReceived = 0;
    std::string strReceived;
    bus.Subscribe<IntEvent>([&](const IntEvent& e) { intReceived = e.value; });
    bus.Subscribe<StringEvent>([&](const StringEvent& e) { strReceived = e.text; });

    bus.Publish(IntEvent{5});
    EXPECT_EQ(intReceived, 5);
    EXPECT_TRUE(strReceived.empty());

    bus.Publish(StringEvent{"hello"});
    EXPECT_EQ(strReceived, "hello");
    EXPECT_EQ(intReceived, 5);
}

TEST(EventBusTest, UnsubscribeStopsDelivery) {
    EventBus bus;
    int callCount = 0;
    uint32_t id = bus.Subscribe<IntEvent>([&](const IntEvent&) { ++callCount; });

    bus.Publish(IntEvent{1});
    EXPECT_EQ(callCount, 1);

    bus.Unsubscribe<IntEvent>(id);
    bus.Publish(IntEvent{2});
    EXPECT_EQ(callCount, 1);
}

TEST(EventBusTest, UnsubscribeOneKeepsOthers) {
    EventBus bus;
    int a = 0;
    int b = 0;
    uint32_t idA = bus.Subscribe<IntEvent>([&](const IntEvent& e) { a = e.value; });
    bus.Subscribe<IntEvent>([&](const IntEvent& e) { b = e.value; });

    bus.Unsubscribe<IntEvent>(idA);
    bus.Publish(IntEvent{9});

    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 9);
}

TEST(EventBusTest, UnsubscribeWithUnknownIdIsNoop) {
    EventBus bus;
    int callCount = 0;
    bus.Subscribe<IntEvent>([&](const IntEvent&) { ++callCount; });

    bus.Unsubscribe<IntEvent>(99999);
    bus.Publish(IntEvent{1});

    EXPECT_EQ(callCount, 1);
}
