#include <gtest/gtest.h>
#include <scene/Snap.h>

#include <vector>

using namespace bimeup::scene;

TEST(SnapTest, NoVerticesReturnsInvalid) {
    std::vector<glm::vec3> verts;
    auto r = SnapToVertex(glm::vec3(0.0f), verts, 1.0f);
    EXPECT_FALSE(r.IsValid());
    EXPECT_EQ(r.type, SnapType::None);
}

TEST(SnapTest, SnapsToNearestVertexWithinThreshold) {
    std::vector<glm::vec3> verts = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
    };
    auto r = SnapToVertex(glm::vec3(0.2f, 0.1f, 0.0f), verts, 1.0f);
    ASSERT_TRUE(r.IsValid());
    EXPECT_EQ(r.type, SnapType::Vertex);
    EXPECT_EQ(r.point, glm::vec3(0.0f));
}

TEST(SnapTest, RejectsVertexOutsideThreshold) {
    std::vector<glm::vec3> verts = {glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f)};
    auto r = SnapToVertex(glm::vec3(5.0f, 0.0f, 0.0f), verts, 1.0f);
    EXPECT_FALSE(r.IsValid());
}

TEST(SnapTest, ChoosesClosestOfMany) {
    std::vector<glm::vec3> verts = {
        glm::vec3(0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f),
        glm::vec3(3.0f, 0.0f, 0.0f),
    };
    auto r = SnapToVertex(glm::vec3(2.1f, 0.0f, 0.0f), verts, 0.5f);
    ASSERT_TRUE(r.IsValid());
    EXPECT_EQ(r.point, glm::vec3(2.0f, 0.0f, 0.0f));
    EXPECT_NEAR(r.distance, 0.1f, 1e-5f);
}

TEST(SnapToEdgeTest, ProjectsOntoEdgeSegment) {
    // Edge from (0,0,0) to (10,0,0). Query above midpoint.
    std::vector<glm::vec3> verts = {glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 5.0f)};
    std::vector<uint32_t> idx = {0, 1, 2};
    auto r = SnapToEdge(glm::vec3(5.0f, 0.2f, 0.0f), verts, idx, 1.0f);
    ASSERT_TRUE(r.IsValid());
    EXPECT_EQ(r.type, SnapType::Edge);
    EXPECT_FLOAT_EQ(r.point.x, 5.0f);
    EXPECT_FLOAT_EQ(r.point.y, 0.0f);
    EXPECT_FLOAT_EQ(r.point.z, 0.0f);
}

TEST(SnapToEdgeTest, ClampsToEndpoint) {
    // Edge segment (0,0,0)-(10,0,0). Query well past the (10,0,0) end.
    std::vector<glm::vec3> verts = {glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 5.0f)};
    std::vector<uint32_t> idx = {0, 1, 2};
    auto r = SnapToEdge(glm::vec3(10.5f, 0.0f, 0.1f), verts, idx, 1.0f);
    ASSERT_TRUE(r.IsValid());
    // Endpoint at x=10
    EXPECT_FLOAT_EQ(r.point.x, 10.0f);
}

TEST(SnapToFaceTest, ProjectsOntoTriangleInterior) {
    // XY plane triangle.
    std::vector<glm::vec3> verts = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
    };
    std::vector<uint32_t> idx = {0, 1, 2};
    // Point above (3,3,0) by 0.5
    auto r = SnapToFace(glm::vec3(3.0f, 3.0f, 0.5f), verts, idx, 1.0f);
    ASSERT_TRUE(r.IsValid());
    EXPECT_EQ(r.type, SnapType::Face);
    EXPECT_FLOAT_EQ(r.point.x, 3.0f);
    EXPECT_FLOAT_EQ(r.point.y, 3.0f);
    EXPECT_FLOAT_EQ(r.point.z, 0.0f);
    EXPECT_FLOAT_EQ(r.distance, 0.5f);
}

TEST(SnapTest, CombinedPrefersVertexOverEdgeOverFace) {
    std::vector<glm::vec3> verts = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
    };
    std::vector<uint32_t> idx = {0, 1, 2};

    // Near vertex (0,0,0) — vertex wins even though edge/face also in range.
    auto r = Snap(glm::vec3(0.05f, 0.05f, 0.0f), verts, idx, 1.0f, 1.0f, 1.0f);
    ASSERT_TRUE(r.IsValid());
    EXPECT_EQ(r.type, SnapType::Vertex);

    // Mid-edge — no vertex within threshold, edge wins.
    auto r2 = Snap(glm::vec3(5.0f, 0.05f, 0.0f), verts, idx, 0.5f, 1.0f, 1.0f);
    ASSERT_TRUE(r2.IsValid());
    EXPECT_EQ(r2.type, SnapType::Edge);

    // Triangle interior — face wins.
    auto r3 = Snap(glm::vec3(3.0f, 3.0f, 0.1f), verts, idx, 0.5f, 0.5f, 1.0f);
    ASSERT_TRUE(r3.IsValid());
    EXPECT_EQ(r3.type, SnapType::Face);
}
