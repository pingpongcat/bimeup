#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "scene/EdgeExtractor.h"

using bimeup::scene::EdgeExtractionConfig;
using bimeup::scene::ExtractedEdges;
using bimeup::scene::ExtractFeatureEdges;

namespace {

// Canonicalise an edge as (min, max) of its endpoints for order-independent
// set comparison.
struct CanonicalEdge {
    glm::vec3 a;
    glm::vec3 b;

    bool operator<(const CanonicalEdge& other) const {
        auto key = [](const glm::vec3& v) {
            return std::tuple<float, float, float>{v.x, v.y, v.z};
        };
        return std::tuple{key(a), key(b)} < std::tuple{key(other.a), key(other.b)};
    }
};

bool NearEq(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return glm::length(a - b) <= eps;
}

// Canonicalise: a < b componentwise-lex, with epsilon equality collapsed.
CanonicalEdge Canon(const glm::vec3& p, const glm::vec3& q) {
    auto lexLess = [](const glm::vec3& u, const glm::vec3& v) {
        if (u.x != v.x) return u.x < v.x;
        if (u.y != v.y) return u.y < v.y;
        return u.z < v.z;
    };
    return lexLess(p, q) ? CanonicalEdge{p, q} : CanonicalEdge{q, p};
}

std::set<CanonicalEdge> ToSet(const ExtractedEdges& edges) {
    std::set<CanonicalEdge> out;
    for (size_t i = 0; i + 1 < edges.indices.size(); i += 2) {
        out.insert(Canon(edges.positions[edges.indices[i]],
                         edges.positions[edges.indices[i + 1]]));
    }
    return out;
}

// A unit cube as 8 shared vertices, 12 triangles, 36 indices.
// All 6 faces are planar and meet at 90° dihedrals — 12 edges expected.
void MakeSharedCube(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) {
    positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},  // z=0
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},  // z=1
    };
    indices = {
        // -Z face (0,3,2,1)
        0, 3, 2,   0, 2, 1,
        // +Z face (4,5,6,7)
        4, 5, 6,   4, 6, 7,
        // -Y face (0,1,5,4)
        0, 1, 5,   0, 5, 4,
        // +Y face (2,3,7,6)
        2, 3, 7,   2, 7, 6,
        // -X face (0,4,7,3)
        0, 4, 7,   0, 7, 3,
        // +X face (1,2,6,5)
        1, 2, 6,   1, 6, 5,
    };
}

// Same cube but flat-shaded: each triangle has its own 3 unique vertices
// (72 entries across positions+indices if one-per-corner; here 24 pos, 36 idx).
// Tests position-welding — without weld every edge looks like a boundary.
void MakeFlatShadedCube(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) {
    std::vector<glm::vec3> shared;
    std::vector<uint32_t> sharedIdx;
    MakeSharedCube(shared, sharedIdx);
    positions.clear();
    indices.clear();
    positions.reserve(sharedIdx.size());
    indices.reserve(sharedIdx.size());
    for (uint32_t i : sharedIdx) {
        indices.push_back(static_cast<uint32_t>(positions.size()));
        positions.push_back(shared[i]);
    }
}

}  // namespace

TEST(EdgeExtractorTest, EmptyInput_ReturnsEmpty) {
    ExtractedEdges edges = ExtractFeatureEdges({}, {}, {});
    EXPECT_TRUE(edges.positions.empty());
    EXPECT_TRUE(edges.indices.empty());
}

TEST(EdgeExtractorTest, DegenerateTriangle_Skipped) {
    // Three collinear points: no valid triangle, nothing to emit.
    std::vector<glm::vec3> positions = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    std::vector<uint32_t> indices = {0, 1, 2};
    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, {});
    EXPECT_TRUE(edges.indices.empty());
}

TEST(EdgeExtractorTest, SingleTriangle_YieldsThreeBoundaryEdges) {
    std::vector<glm::vec3> positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::vector<uint32_t> indices = {0, 1, 2};
    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, {});

    auto set = ToSet(edges);
    EXPECT_EQ(set.size(), 3u);
    EXPECT_TRUE(set.count(Canon({0, 0, 0}, {1, 0, 0})));
    EXPECT_TRUE(set.count(Canon({1, 0, 0}, {0, 1, 0})));
    EXPECT_TRUE(set.count(Canon({0, 1, 0}, {0, 0, 0})));
}

TEST(EdgeExtractorTest, CoplanarQuad_DropsInteriorDiagonal) {
    // Two triangles sharing the diagonal (0,0,0)-(1,1,0); both lie in z=0.
    // Dihedral angle = 0° ⇒ diagonal must be dropped; 4 boundary edges remain.
    std::vector<glm::vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    };
    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, {});

    auto set = ToSet(edges);
    EXPECT_EQ(set.size(), 4u);
    EXPECT_FALSE(set.count(Canon({0, 0, 0}, {1, 1, 0})));  // diagonal dropped
    EXPECT_TRUE(set.count(Canon({0, 0, 0}, {1, 0, 0})));
    EXPECT_TRUE(set.count(Canon({1, 0, 0}, {1, 1, 0})));
    EXPECT_TRUE(set.count(Canon({1, 1, 0}, {0, 1, 0})));
    EXPECT_TRUE(set.count(Canon({0, 1, 0}, {0, 0, 0})));
}

TEST(EdgeExtractorTest, SharedVertexCube_YieldsTwelveEdges) {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeSharedCube(positions, indices);

    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, {});
    auto set = ToSet(edges);

    EXPECT_EQ(set.size(), 12u);
    // Spot-check the 4 bottom-face edges.
    EXPECT_TRUE(set.count(Canon({0, 0, 0}, {1, 0, 0})));
    EXPECT_TRUE(set.count(Canon({1, 0, 0}, {1, 1, 0})));
    EXPECT_TRUE(set.count(Canon({1, 1, 0}, {0, 1, 0})));
    EXPECT_TRUE(set.count(Canon({0, 1, 0}, {0, 0, 0})));
    // No face-diagonal should slip through.
    EXPECT_FALSE(set.count(Canon({0, 0, 0}, {1, 1, 0})));
    EXPECT_FALSE(set.count(Canon({0, 0, 1}, {1, 1, 1})));
}

TEST(EdgeExtractorTest, FlatShadedCube_WeldsAndYieldsTwelveEdges) {
    // Same geometry but every triangle has private vertex copies.
    // Without position-welding the extractor would report 36 boundary edges;
    // with welding it must still produce the 12 true edges.
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeFlatShadedCube(positions, indices);
    ASSERT_EQ(positions.size(), 36u);  // 12 tris * 3 corners — no sharing

    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, {});
    auto set = ToSet(edges);

    EXPECT_EQ(set.size(), 12u);
}

TEST(EdgeExtractorTest, HighThreshold_DropsShallowEdges) {
    // Two triangles meeting at a shallow 10° angle along the y-axis.
    // Threshold 30° ⇒ seam dropped; both triangles' outer boundaries remain.
    const float angleDeg = 10.0f;
    const float h = std::tan(glm::radians(angleDeg));  // rise across unit x

    std::vector<glm::vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {0, 1, 0},  // tri A in z=0 plane (normal +Z)
        {-1, 0, h}, {-1, 1, h},           // tri B tilts slightly
    };
    // Tri A: 0,1,2   Tri B: 0,2,4,  0,4,3  (sharing edge 0-2 with A)
    std::vector<uint32_t> indices = {
        0, 1, 2,
        0, 2, 4,
        0, 4, 3,
    };

    EdgeExtractionConfig config;
    config.dihedralAngleDegrees = 30.0f;
    ExtractedEdges edges = ExtractFeatureEdges(positions, indices, config);
    auto set = ToSet(edges);

    // Shared 0-2 seam is shallow ⇒ dropped.
    EXPECT_FALSE(set.count(Canon({0, 0, 0}, {0, 1, 0})));
}

TEST(EdgeExtractorTest, Deterministic_SameInputSameOutput) {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeSharedCube(positions, indices);

    ExtractedEdges a = ExtractFeatureEdges(positions, indices, {});
    ExtractedEdges b = ExtractFeatureEdges(positions, indices, {});

    ASSERT_EQ(a.positions.size(), b.positions.size());
    ASSERT_EQ(a.indices.size(), b.indices.size());
    for (size_t i = 0; i < a.positions.size(); ++i) {
        EXPECT_TRUE(NearEq(a.positions[i], b.positions[i]));
    }
    for (size_t i = 0; i < a.indices.size(); ++i) {
        EXPECT_EQ(a.indices[i], b.indices[i]);
    }
}
