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
using bimeup::scene::StitchResult;
using bimeup::scene::StitchSegments;
using bimeup::scene::StitchSegmentsDetailed;
using bimeup::scene::TriangleCut;
using bimeup::scene::TriangulatePolygon;

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

// Fix #3 prep — endpoint welding. Two endpoints within `epsilon` that happen
// to fall in adjacent quantization cells (one rounds down, the other rounds
// up) used to silently fail to match, which kept arched-roof sections from
// closing. Neighbour-cell lookup now bridges the gap.
TEST(StitchSegments, MatchesEndpointsAcrossQuantizationBoundary) {
    // epsilon = 1e-4 → quantization step = 1e-4. Points 0.9999 and 1.0
    // land in cells 9999 vs 10000 (distance = epsilon).
    const std::vector<Segment> segs = {
        {{0, 0, 0}, {0.9999F, 0, 0}},
        {{1.0F, 0, 0}, {1.0F, 0, 1.0F}},
        {{1.0F, 0, 1.0F}, {0.0F, 0, 1.0F}},
        {{0.0F, 0, 1.0F}, {0.0F, 0, 0.0F}},
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

namespace {

// Approximate unsigned area of a 3D planar polygon via fan triangulation.
float PolygonArea3D(const std::vector<glm::vec3>& poly) {
    if (poly.size() < 3) return 0.0F;
    glm::vec3 sum(0.0F);
    for (size_t i = 1; i + 1 < poly.size(); ++i) {
        sum += glm::cross(poly[i] - poly[0], poly[i + 1] - poly[0]);
    }
    return 0.5F * glm::length(sum);
}

float TrianglesArea(const std::vector<glm::vec3>& tris) {
    float total = 0.0F;
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        total += 0.5F * glm::length(
                            glm::cross(tris[i + 1] - tris[i], tris[i + 2] - tris[i]));
    }
    return total;
}

}  // namespace

TEST(TriangulatePolygon, DegeneratePolygonsProduceNoTriangles) {
    EXPECT_TRUE(TriangulatePolygon({}, {0, 1, 0}).empty());
    EXPECT_TRUE(TriangulatePolygon({{0, 0, 0}}, {0, 1, 0}).empty());
    EXPECT_TRUE(TriangulatePolygon({{0, 0, 0}, {1, 0, 0}}, {0, 1, 0}).empty());
}

TEST(TriangulatePolygon, UnitSquareProducesTwoTriangles) {
    // CCW unit square at y=0.
    const std::vector<glm::vec3> poly = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
    };
    const auto tris = TriangulatePolygon(poly, {0, 1, 0});
    ASSERT_EQ(tris.size(), 6U);  // 2 triangles
    EXPECT_NEAR(TrianglesArea(tris), 1.0F, 1e-4F);
}

TEST(TriangulatePolygon, ClockwiseInputStillProducesTrianglesCoveringArea) {
    // CW input — implementation should detect and handle either winding.
    const std::vector<glm::vec3> poly = {
        {0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0},
    };
    const auto tris = TriangulatePolygon(poly, {0, 1, 0});
    ASSERT_EQ(tris.size(), 6U);
    EXPECT_NEAR(TrianglesArea(tris), 1.0F, 1e-4F);
}

TEST(TriangulatePolygon, ConcaveLShapeProducesFourTriangles) {
    // L-shape with 6 vertices in xz-plane, CCW:
    //   (0,0)-(2,0)-(2,1)-(1,1)-(1,2)-(0,2). Area = 3.
    const std::vector<glm::vec3> poly = {
        {0, 0, 0}, {2, 0, 0}, {2, 0, 1}, {1, 0, 1}, {1, 0, 2}, {0, 0, 2},
    };
    const auto tris = TriangulatePolygon(poly, {0, 1, 0});
    ASSERT_EQ(tris.size(), 12U);  // n-2 = 4 triangles
    EXPECT_NEAR(TrianglesArea(tris), 3.0F, 1e-4F);
    EXPECT_NEAR(PolygonArea3D(poly), 3.0F, 1e-4F);
}

TEST(TriangulatePolygon, OutputCoversInputAreaForArbitraryPlane) {
    // Triangle spanning an oblique plane (normal roughly (1,1,1)).
    const std::vector<glm::vec3> poly = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
    };
    const glm::vec3 normal = glm::normalize(glm::vec3{1, 1, 1});
    const auto tris = TriangulatePolygon(poly, normal);
    ASSERT_EQ(tris.size(), 3U);  // one triangle
    EXPECT_NEAR(TrianglesArea(tris), PolygonArea3D(poly), 1e-4F);
}

TEST(TriangulatePolygon, UsesOriginalVerticesInOutput) {
    // Each output vertex should equal one of the input vertices (bit-for-bit,
    // since we re-index — no interpolation happens in ear-clipping).
    const std::vector<glm::vec3> poly = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
    };
    const auto tris = TriangulatePolygon(poly, {0, 1, 0});
    for (const auto& v : tris) {
        const bool found = std::any_of(poly.begin(), poly.end(),
                                       [&](const glm::vec3& p) { return NearEq(v, p); });
        EXPECT_TRUE(found) << "output vertex (" << v.x << "," << v.y << "," << v.z
                           << ") not in input polygon";
    }
}

// Fix #3 — arched-roof cross sections. Ear-clip has been known to refuse
// ears near tight-angle concavities on dense polygons. poly2tri's constrained
// Delaunay handles the same input robustly. This test closes a half-disk
// profile (30-point arc + implicit closing chord) and verifies the output
// triangles cover the analytical area of π/2.
TEST(TriangulatePolygon, ArchProfileCoversAnalyticalArea) {
    std::vector<glm::vec3> polygon;
    constexpr int kN = 30;
    polygon.reserve(kN + 1);
    for (int i = 0; i <= kN; ++i) {
        const float t = glm::pi<float>() * static_cast<float>(i) /
                        static_cast<float>(kN);
        polygon.push_back({std::cos(t), 0.0F, std::sin(t)});
    }
    // First pt (1,0,0), last pt (-1,0,0); closing edge runs along y=0.

    const auto tris = TriangulatePolygon(polygon, {0, 1, 0});
    ASSERT_EQ(tris.size() % 3, 0U);
    ASSERT_GE(tris.size(), 3U * (polygon.size() - 2));

    const float area = TrianglesArea(tris);
    EXPECT_NEAR(area, glm::pi<float>() / 2.0F, 5e-2F);
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

TEST(StitchSegmentsDetailed, SeparatesClosedAndOpenPolylines) {
    // Closed unit square + a disjoint 3-segment open polyline.
    const std::vector<Segment> segs = {
        {{0, 0, 0}, {1, 0, 0}},
        {{1, 0, 0}, {1, 0, 1}},
        {{1, 0, 1}, {0, 0, 1}},
        {{0, 0, 1}, {0, 0, 0}},
        {{5, 0, 0}, {6, 0, 0}},
        {{6, 0, 0}, {6, 0, 1}},
        {{6, 0, 1}, {5, 0, 1}},
    };
    const StitchResult result = StitchSegmentsDetailed(segs);
    ASSERT_EQ(result.closed.size(), 1U);
    EXPECT_EQ(result.closed[0].size(), 4U);
    ASSERT_EQ(result.open.size(), 1U);
    // Open polyline collects 4 unique vertices along its path.
    EXPECT_EQ(result.open[0].size(), 4U);
}

TEST(StitchSegmentsDetailed, LegacyWrapperReturnsOnlyClosed) {
    const std::vector<Segment> segs = {
        {{5, 0, 0}, {6, 0, 0}},
        {{6, 0, 0}, {6, 0, 1}},
        {{6, 0, 1}, {5, 0, 1}},
    };
    EXPECT_TRUE(StitchSegments(segs).empty());
    EXPECT_EQ(StitchSegmentsDetailed(segs).open.size(), 1U);
}
