#include <gtest/gtest.h>
#include <scene/Measurement.h>

#include <cmath>

using namespace bimeup::scene;

TEST(MeasurementTest, DistanceBetweenSamePointIsZero) {
    auto r = Measure(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(r.distance, 0.0f);
    EXPECT_EQ(r.deltaXYZ, glm::vec3(0.0f));
}

TEST(MeasurementTest, AxisAlignedDistance) {
    auto r = Measure(glm::vec3(0.0f), glm::vec3(3.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(r.distance, 3.0f);
    EXPECT_EQ(r.deltaXYZ, glm::vec3(3.0f, 0.0f, 0.0f));
    EXPECT_EQ(r.pointA, glm::vec3(0.0f));
    EXPECT_EQ(r.pointB, glm::vec3(3.0f, 0.0f, 0.0f));
}

TEST(MeasurementTest, DiagonalDistanceMatchesPythagoras) {
    auto r = Measure(glm::vec3(0.0f), glm::vec3(3.0f, 4.0f, 0.0f));
    EXPECT_FLOAT_EQ(r.distance, 5.0f);
    EXPECT_EQ(r.deltaXYZ, glm::vec3(3.0f, 4.0f, 0.0f));
}

TEST(MeasurementTest, ThreeDimensionalDistance) {
    // 1,2,2 → sqrt(1+4+4) = 3
    auto r = Measure(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(2.0f, 3.0f, 3.0f));
    EXPECT_FLOAT_EQ(r.distance, 3.0f);
    EXPECT_EQ(r.deltaXYZ, glm::vec3(1.0f, 2.0f, 2.0f));
}

TEST(MeasurementTest, DeltaIsSignedBMinusA) {
    auto r = Measure(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(2.0f, 3.0f, 1.0f));
    EXPECT_EQ(r.deltaXYZ, glm::vec3(-3.0f, -2.0f, -4.0f));
    EXPECT_FLOAT_EQ(r.distance, std::sqrt(9.0f + 4.0f + 16.0f));
}

TEST(MeasureToolTest, StartsIdle) {
    MeasureTool tool;
    EXPECT_EQ(tool.GetState(), MeasureTool::State::Idle);
    EXPECT_FALSE(tool.GetResult().has_value());
}

TEST(MeasureToolTest, FirstClickStoresPointAndAwaitsSecond) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(tool.GetState(), MeasureTool::State::AwaitingSecondPoint);
    EXPECT_FALSE(tool.GetResult().has_value());
    ASSERT_TRUE(tool.GetFirstPoint().has_value());
    EXPECT_EQ(*tool.GetFirstPoint(), glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(MeasureToolTest, SecondClickProducesResult) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(0.0f, 0.0f, 4.0f));
    EXPECT_EQ(tool.GetState(), MeasureTool::State::Complete);
    ASSERT_TRUE(tool.GetResult().has_value());
    EXPECT_FLOAT_EQ(tool.GetResult()->distance, 4.0f);
}

TEST(MeasureToolTest, ResetReturnsToIdle) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    tool.Reset();
    EXPECT_EQ(tool.GetState(), MeasureTool::State::Idle);
    EXPECT_FALSE(tool.GetResult().has_value());
    EXPECT_FALSE(tool.GetFirstPoint().has_value());
}

TEST(MeasureToolTest, AddingPointAfterCompleteStartsNewMeasurement) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    tool.AddPoint(glm::vec3(5.0f, 0.0f, 0.0f));
    EXPECT_EQ(tool.GetState(), MeasureTool::State::AwaitingSecondPoint);
    ASSERT_TRUE(tool.GetFirstPoint().has_value());
    EXPECT_EQ(*tool.GetFirstPoint(), glm::vec3(5.0f, 0.0f, 0.0f));
    EXPECT_FALSE(tool.GetResult().has_value());
}

TEST(MeasureToolTest, CancelClearsInProgressMeasurement) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.Cancel();
    EXPECT_EQ(tool.GetState(), MeasureTool::State::Idle);
    EXPECT_FALSE(tool.GetFirstPoint().has_value());
}

TEST(MeasureToolTest, CompletedMeasurementIsRecorded) {
    MeasureTool tool;
    EXPECT_TRUE(tool.GetMeasurements().empty());
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(3.0f, 0.0f, 0.0f));
    ASSERT_EQ(tool.GetMeasurements().size(), 1u);
    EXPECT_FLOAT_EQ(tool.GetMeasurements()[0].distance, 3.0f);
}

TEST(MeasureToolTest, CancelAfterCompletePreservesRecordedHistory) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(3.0f, 0.0f, 0.0f));
    tool.AddPoint(glm::vec3(10.0f, 0.0f, 0.0f));  // start a 2nd, only first point set
    tool.Cancel();
    EXPECT_EQ(tool.GetMeasurements().size(), 1u);
    EXPECT_EQ(tool.GetState(), MeasureTool::State::Idle);
}

TEST(MeasureToolTest, ClearMeasurementsErasesHistory) {
    MeasureTool tool;
    tool.AddPoint(glm::vec3(0.0f));
    tool.AddPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    tool.ClearMeasurements();
    EXPECT_TRUE(tool.GetMeasurements().empty());
}
