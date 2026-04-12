#include <gtest/gtest.h>
#include <scene/AABB.h>

#include <vector>

using namespace bimeup::scene;

TEST(AABBTest, DefaultConstructionIsInvalid) {
    AABB box;
    EXPECT_FALSE(box.IsValid());
}

TEST(AABBTest, ConstructFromMinMax) {
    AABB box(glm::vec3(0.0f), glm::vec3(1.0f));
    EXPECT_TRUE(box.IsValid());
    EXPECT_EQ(box.GetMin(), glm::vec3(0.0f));
    EXPECT_EQ(box.GetMax(), glm::vec3(1.0f));
}

TEST(AABBTest, ComputeFromVertices) {
    std::vector<glm::vec3> vertices = {
        {-1.0f, 0.0f, 0.0f},
        { 2.0f, 3.0f, 1.0f},
        { 0.0f,-2.0f, 5.0f},
    };

    AABB box = AABB::FromVertices(vertices);
    EXPECT_TRUE(box.IsValid());
    EXPECT_EQ(box.GetMin(), glm::vec3(-1.0f, -2.0f, 0.0f));
    EXPECT_EQ(box.GetMax(), glm::vec3(2.0f, 3.0f, 5.0f));
}

TEST(AABBTest, FromVerticesEmptyReturnsInvalid) {
    AABB box = AABB::FromVertices({});
    EXPECT_FALSE(box.IsValid());
}

TEST(AABBTest, MergeTwoAABBs) {
    AABB a(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
    AABB b(glm::vec3(-1.0f, 2.0f, -3.0f), glm::vec3(0.5f, 4.0f, 0.0f));

    AABB merged = AABB::Merge(a, b);
    EXPECT_TRUE(merged.IsValid());
    EXPECT_EQ(merged.GetMin(), glm::vec3(-1.0f, 0.0f, -3.0f));
    EXPECT_EQ(merged.GetMax(), glm::vec3(1.0f, 4.0f, 1.0f));
}

TEST(AABBTest, MergeWithInvalidReturnsOther) {
    AABB valid(glm::vec3(0.0f), glm::vec3(1.0f));
    AABB invalid;

    AABB merged1 = AABB::Merge(valid, invalid);
    EXPECT_EQ(merged1.GetMin(), valid.GetMin());
    EXPECT_EQ(merged1.GetMax(), valid.GetMax());

    AABB merged2 = AABB::Merge(invalid, valid);
    EXPECT_EQ(merged2.GetMin(), valid.GetMin());
    EXPECT_EQ(merged2.GetMax(), valid.GetMax());
}

TEST(AABBTest, MergeTwoInvalidReturnsInvalid) {
    AABB a;
    AABB b;
    AABB merged = AABB::Merge(a, b);
    EXPECT_FALSE(merged.IsValid());
}

TEST(AABBTest, ExpandByPoint) {
    AABB box(glm::vec3(0.0f), glm::vec3(1.0f));
    box.ExpandToInclude(glm::vec3(3.0f, -1.0f, 0.5f));

    EXPECT_EQ(box.GetMin(), glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_EQ(box.GetMax(), glm::vec3(3.0f, 1.0f, 1.0f));
}

TEST(AABBTest, ExpandInvalidByPointMakesValid) {
    AABB box;
    box.ExpandToInclude(glm::vec3(5.0f, 3.0f, 1.0f));

    EXPECT_TRUE(box.IsValid());
    EXPECT_EQ(box.GetMin(), glm::vec3(5.0f, 3.0f, 1.0f));
    EXPECT_EQ(box.GetMax(), glm::vec3(5.0f, 3.0f, 1.0f));
}

TEST(AABBTest, ContainsPoint) {
    AABB box(glm::vec3(0.0f), glm::vec3(2.0f));

    EXPECT_TRUE(box.Contains(glm::vec3(1.0f)));
    EXPECT_TRUE(box.Contains(glm::vec3(0.0f)));   // on boundary
    EXPECT_TRUE(box.Contains(glm::vec3(2.0f)));   // on boundary
    EXPECT_FALSE(box.Contains(glm::vec3(3.0f, 1.0f, 1.0f)));
    EXPECT_FALSE(box.Contains(glm::vec3(-0.1f, 1.0f, 1.0f)));
}

TEST(AABBTest, CenterAndSize) {
    AABB box(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(3.0f, 6.0f, 7.0f));

    EXPECT_EQ(box.GetCenter(), glm::vec3(2.0f, 4.0f, 5.0f));
    EXPECT_EQ(box.GetSize(), glm::vec3(2.0f, 4.0f, 4.0f));
}
