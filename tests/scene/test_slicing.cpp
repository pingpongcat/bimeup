#include <gtest/gtest.h>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/ClipPlane.h"
#include "scene/SceneMesh.h"
#include "scene/Slicing.h"

using bimeup::renderer::ClipPlane;
using bimeup::scene::SceneMesh;
using bimeup::scene::Segment;
using bimeup::scene::SliceSceneMesh;
using bimeup::scene::SliceTriangle;
using bimeup::scene::StitchSegments;
using bimeup::scene::TriangleCut;

namespace {

// Plane y=0 (normal +Y, d=0): front = y>0, back = y<0.
ClipPlane YZero() {
    return ClipPlane::FromPointNormal({0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
}

bool NearEq(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4F) {
    return glm::length(a - b) <= eps;
}

// Triangle has an unordered pair of intersection points equal to {p, q}.
bool HasPoints(const TriangleCut& cut, const glm::vec3& p, const glm::vec3& q) {
    if (cut.pointCount != 2) return false;
    return (NearEq(cut.points[0], p) && NearEq(cut.points[1], q)) ||
           (NearEq(cut.points[0], q) && NearEq(cut.points[1], p));
}

}  // namespace

TEST(SliceTriangle, AllFrontProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, 1, 0}, {1, 1, 0}, {0, 1, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, AllBackProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, -1, 0}, {1, -1, 0}, {0, -1, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, CoplanarProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 0, 0}, {0, 0, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, TwoFrontOneBackCutsBothStraddlingEdges) {
    // A=(0,1,0) front, B=(2,1,0) front, C=(1,-1,0) back.
    // Edge BC crosses at midpoint (1.5, 0, 0). Edge CA crosses at (0.5, 0, 0).
    const auto cut = SliceTriangle(YZero(), {0, 1, 0}, {2, 1, 0}, {1, -1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.5F, 0.0F, 0.0F}, {1.5F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, TwoBackOneFrontCutsBothStraddlingEdges) {
    // A=(0,-1,0) back, B=(2,-1,0) back, C=(1,1,0) front.
    const auto cut = SliceTriangle(YZero(), {0, -1, 0}, {2, -1, 0}, {1, 1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.5F, 0.0F, 0.0F}, {1.5F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, VertexOnPlaneStraddleUsesVertexAndOppositeEdge) {
    // A=(0,0,0) on plane, B=(1,1,0) front, C=(1,-1,0) back.
    // Segment goes from A to mid of BC = (1, 0, 0).
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 1, 0}, {1, -1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, VertexOnPlaneButNoStraddleDrops) {
    // A=(0,0,0) on plane, rest on the same (front) side → single-point touch.
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 1, 0}, {2, 1, 0});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, EdgeOnPlaneReturnsBothOnPlaneVertices) {
    // A=(0,0,0) and B=(1,0,0) on plane; C=(0,1,0) front. The edge AB lies on
    // the plane — emit it as the section segment.
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 0, 0}, {0, 1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}));
}

namespace {

// Unit cube [0,1]^3 as 8 vertices + 12 triangles.
SceneMesh MakeUnitCubeMesh() {
    SceneMesh mesh;
    mesh.SetPositions({
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},  // z=0 face verts 0..3
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},  // z=1 face verts 4..7
    });
    mesh.SetIndices({
        0, 2, 1, 0, 3, 2,  // z=0
        4, 5, 6, 4, 6, 7,  // z=1
        0, 1, 5, 0, 5, 4,  // y=0
        2, 3, 7, 2, 7, 6,  // y=1
        0, 4, 7, 0, 7, 3,  // x=0
        1, 2, 6, 1, 6, 5,  // x=1
    });
    return mesh;
}

bool HasSegmentEndpoints(const std::vector<Segment>& segs,
                         const glm::vec3& p,
                         const glm::vec3& q,
                         float eps = 1e-4F) {
    return std::any_of(segs.begin(), segs.end(), [&](const Segment& s) {
        return (NearEq(s.a, p, eps) && NearEq(s.b, q, eps)) ||
               (NearEq(s.a, q, eps) && NearEq(s.b, p, eps));
    });
}

}  // namespace

TEST(SliceSceneMesh, EmptyMeshProducesNoSegments) {
    SceneMesh mesh;
    const auto segs = SliceSceneMesh(mesh, glm::mat4(1.0F), YZero());
    EXPECT_TRUE(segs.empty());
}

TEST(SliceSceneMesh, AllAboveProducesNoSegments) {
    const auto mesh = MakeUnitCubeMesh();
    // Plane at y=-1 — cube is entirely above.
    const auto plane =
        ClipPlane::FromPointNormal({0, -1, 0}, {0, 1, 0});
    const auto segs = SliceSceneMesh(mesh, glm::mat4(1.0F), plane);
    EXPECT_TRUE(segs.empty());
}

TEST(SliceSceneMesh, UnitCubeAtYHalfProducesEightPerimeterSegments) {
    const auto mesh = MakeUnitCubeMesh();
    // Plane y=0.5 cuts the cube through the middle. Each of the 4 side faces
    // is 2 triangles, and both triangles straddle the plane — 8 segments
    // total (7.5e will stitch them into a unit square).
    const auto plane =
        ClipPlane::FromPointNormal({0, 0.5F, 0}, {0, 1, 0});
    const auto segs = SliceSceneMesh(mesh, glm::mat4(1.0F), plane);

    EXPECT_EQ(segs.size(), 8U);

    // All endpoints lie exactly at y=0.5 and on the unit-square perimeter
    // at y=0.5 (x or z pinned to 0 or 1).
    auto onPerimeter = [](const glm::vec3& p) {
        const bool atY = std::abs(p.y - 0.5F) <= 1e-4F;
        const bool atXEdge = std::abs(p.x) <= 1e-4F || std::abs(p.x - 1.0F) <= 1e-4F;
        const bool atZEdge = std::abs(p.z) <= 1e-4F || std::abs(p.z - 1.0F) <= 1e-4F;
        return atY && (atXEdge || atZEdge);
    };
    for (const auto& s : segs) {
        EXPECT_TRUE(onPerimeter(s.a)) << "a=" << s.a.x << "," << s.a.y << "," << s.a.z;
        EXPECT_TRUE(onPerimeter(s.b)) << "b=" << s.b.x << "," << s.b.y << "," << s.b.z;
    }

    // Total segment length should equal the perimeter of the unit square = 4.
    float totalLen = 0.0F;
    for (const auto& s : segs) totalLen += glm::length(s.b - s.a);
    EXPECT_NEAR(totalLen, 4.0F, 1e-3F);
}

namespace {

// Sum of edge lengths around a polygon, including the implicit closing edge.
float PolygonPerimeter(const std::vector<glm::vec3>& poly) {
    float len = 0.0F;
    if (poly.size() < 2) return len;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % poly.size()];
        len += glm::length(q - p);
    }
    return len;
}

bool PolygonContains(const std::vector<glm::vec3>& poly,
                     const glm::vec3& p,
                     float eps = 1e-3F) {
    return std::any_of(poly.begin(), poly.end(),
                       [&](const glm::vec3& v) { return NearEq(v, p, eps); });
}

}  // namespace

TEST(StitchSegments, EmptyInputProducesNoPolygons) {
    std::vector<Segment> segs;
    const auto polys = StitchSegments(segs);
    EXPECT_TRUE(polys.empty());
}

TEST(StitchSegments, FourSegmentsOfUnitSquareProduceOneQuadPolygon) {
    // Counter-clockwise unit square in the xz-plane at y=0.
    const std::vector<Segment> segs = {
        {{0, 0, 0}, {1, 0, 0}},
        {{1, 0, 0}, {1, 0, 1}},
        {{1, 0, 1}, {0, 0, 1}},
        {{0, 0, 1}, {0, 0, 0}},
    };
    const auto polys = StitchSegments(segs);
    ASSERT_EQ(polys.size(), 1U);
    EXPECT_EQ(polys[0].size(), 4U);
    EXPECT_NEAR(PolygonPerimeter(polys[0]), 4.0F, 1e-3F);
    EXPECT_TRUE(PolygonContains(polys[0], {0, 0, 0}));
    EXPECT_TRUE(PolygonContains(polys[0], {1, 0, 0}));
    EXPECT_TRUE(PolygonContains(polys[0], {1, 0, 1}));
    EXPECT_TRUE(PolygonContains(polys[0], {0, 0, 1}));
}

TEST(StitchSegments, ShuffledAndFlippedSegmentsStillStitchSquare) {
    // Same square, but each segment is in arbitrary order/direction.
    const std::vector<Segment> segs = {
        {{1, 0, 1}, {1, 0, 0}},  // flipped
        {{0, 0, 0}, {0, 0, 1}},  // flipped
        {{0, 0, 0}, {1, 0, 0}},
        {{0, 0, 1}, {1, 0, 1}},  // flipped
    };
    const auto polys = StitchSegments(segs);
    ASSERT_EQ(polys.size(), 1U);
    EXPECT_EQ(polys[0].size(), 4U);
    EXPECT_NEAR(PolygonPerimeter(polys[0]), 4.0F, 1e-3F);
}

TEST(StitchSegments, TwoDisjointSquaresProduceTwoPolygons) {
    const std::vector<Segment> segs = {
        // Square 1 at origin.
        {{0, 0, 0}, {1, 0, 0}},
        {{1, 0, 0}, {1, 0, 1}},
        {{1, 0, 1}, {0, 0, 1}},
        {{0, 0, 1}, {0, 0, 0}},
        // Square 2 shifted to x=10.
        {{10, 0, 0}, {11, 0, 0}},
        {{11, 0, 0}, {11, 0, 1}},
        {{11, 0, 1}, {10, 0, 1}},
        {{10, 0, 1}, {10, 0, 0}},
    };
    const auto polys = StitchSegments(segs);
    ASSERT_EQ(polys.size(), 2U);
    EXPECT_EQ(polys[0].size(), 4U);
    EXPECT_EQ(polys[1].size(), 4U);
    EXPECT_NEAR(PolygonPerimeter(polys[0]), 4.0F, 1e-3F);
    EXPECT_NEAR(PolygonPerimeter(polys[1]), 4.0F, 1e-3F);
}

TEST(StitchSegments, OpenPolylineIsDropped) {
    // Three connected segments that don't close back to the start.
    const std::vector<Segment> segs = {
        {{0, 0, 0}, {1, 0, 0}},
        {{1, 0, 0}, {1, 0, 1}},
        {{1, 0, 1}, {0, 0, 1}},
    };
    const auto polys = StitchSegments(segs);
    EXPECT_TRUE(polys.empty());
}

TEST(StitchSegments, EpsilonSnapsNearlyMatchingEndpoints) {
    // Endpoints differ by 1e-5, well under the default epsilon (1e-4).
    const std::vector<Segment> segs = {
        {{0, 0, 0}, {1, 0, 0}},
        {{1.000005F, 0, 0}, {1, 0, 1}},
        {{1, 0, 1.000005F}, {0, 0, 1}},
        {{0, 0, 1}, {0.000005F, 0, 0}},
    };
    const auto polys = StitchSegments(segs);
    ASSERT_EQ(polys.size(), 1U);
    EXPECT_EQ(polys[0].size(), 4U);
}

TEST(StitchSegments, CubeSliceStitchesIntoSingleClosedLoop) {
    // The 8 segments produced by SliceSceneMesh on a unit cube at y=0.5 stitch
    // into one closed loop. Each side face contributes two segments meeting at
    // a midpoint along the face diagonal, so the loop has 8 vertices (4 cube
    // corners + 4 edge-midpoints) and perimeter == 4.
    const auto mesh = MakeUnitCubeMesh();
    const auto plane = ClipPlane::FromPointNormal({0, 0.5F, 0}, {0, 1, 0});
    const auto segs = SliceSceneMesh(mesh, glm::mat4(1.0F), plane);
    ASSERT_EQ(segs.size(), 8U);

    const auto polys = StitchSegments(segs);
    ASSERT_EQ(polys.size(), 1U);
    EXPECT_EQ(polys[0].size(), 8U);
    EXPECT_NEAR(PolygonPerimeter(polys[0]), 4.0F, 1e-3F);
}

TEST(SliceSceneMesh, AppliesWorldTransformToPositions) {
    const auto mesh = MakeUnitCubeMesh();
    // Translate the cube to x=+10, then cut at y=0.5. All resulting segments
    // must have x in [10, 11] (shifted perimeter).
    const glm::mat4 xform = glm::translate(glm::mat4(1.0F), glm::vec3{10, 0, 0});
    const auto plane =
        ClipPlane::FromPointNormal({0, 0.5F, 0}, {0, 1, 0});
    const auto segs = SliceSceneMesh(mesh, xform, plane);

    EXPECT_EQ(segs.size(), 8U);
    for (const auto& s : segs) {
        EXPECT_GE(s.a.x, 10.0F - 1e-4F);
        EXPECT_LE(s.a.x, 11.0F + 1e-4F);
        EXPECT_GE(s.b.x, 10.0F - 1e-4F);
        EXPECT_LE(s.b.x, 11.0F + 1e-4F);
        EXPECT_NEAR(s.a.y, 0.5F, 1e-4F);
        EXPECT_NEAR(s.b.y, 0.5F, 1e-4F);
    }
}
