#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include "scene/EdgeExtractor.h"

using bimeup::scene::EdgeExtractionConfig;
using bimeup::scene::ExtractFeatureEdges;

namespace {

bool NearEq(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return glm::length(a - b) <= eps;
}

// Edge indices reference the input `positions` array. Collapse them to a
// set of ordered (a, b) position-pairs (lexicographic-smallest first) so
// tests can assert on geometry rather than on index identity.
struct GeomEdge {
    glm::vec3 a;
    glm::vec3 b;

    bool operator<(const GeomEdge& other) const {
        auto key = [](const glm::vec3& v) {
            return std::tuple<float, float, float>{v.x, v.y, v.z};
        };
        return std::tuple{key(a), key(b)} < std::tuple{key(other.a), key(other.b)};
    }
};

GeomEdge Canon(const glm::vec3& p, const glm::vec3& q) {
    auto lexLess = [](const glm::vec3& u, const glm::vec3& v) {
        if (u.x != v.x) return u.x < v.x;
        if (u.y != v.y) return u.y < v.y;
        return u.z < v.z;
    };
    return lexLess(p, q) ? GeomEdge{p, q} : GeomEdge{q, p};
}

std::set<GeomEdge> ToSet(const std::vector<glm::vec3>& positions,
                         const std::vector<uint32_t>& edgeIndices) {
    std::set<GeomEdge> out;
    for (size_t i = 0; i + 1 < edgeIndices.size(); i += 2) {
        out.insert(Canon(positions[edgeIndices[i]], positions[edgeIndices[i + 1]]));
    }
    return out;
}

// A unit cube as 8 shared vertices, 12 triangles, 36 indices.
void MakeSharedCube(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) {
    positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
    };
    indices = {
        0, 3, 2,   0, 2, 1,
        4, 5, 6,   4, 6, 7,
        0, 1, 5,   0, 5, 4,
        2, 3, 7,   2, 7, 6,
        0, 4, 7,   0, 7, 3,
        1, 2, 6,   1, 6, 5,
    };
}

// Same geometry, flat-shaded: every triangle has its own 3 unique vertices.
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
    auto edges = ExtractFeatureEdges({}, {}, {});
    EXPECT_TRUE(edges.empty());
}

TEST(EdgeExtractorTest, DegenerateTriangle_Skipped) {
    std::vector<glm::vec3> positions = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    std::vector<uint32_t> indices = {0, 1, 2};
    auto edges = ExtractFeatureEdges(positions, indices, {});
    EXPECT_TRUE(edges.empty());
}

TEST(EdgeExtractorTest, SingleTriangle_YieldsThreeBoundaryEdges) {
    std::vector<glm::vec3> positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::vector<uint32_t> indices = {0, 1, 2};
    auto edges = ExtractFeatureEdges(positions, indices, {});

    ASSERT_EQ(edges.size(), 6u);  // 3 edges × 2 indices each
    for (auto idx : edges) {
        EXPECT_LT(idx, positions.size());
    }
    auto set = ToSet(positions, edges);
    EXPECT_EQ(set.size(), 3u);
    EXPECT_TRUE(set.count(Canon({0, 0, 0}, {1, 0, 0})));
    EXPECT_TRUE(set.count(Canon({1, 0, 0}, {0, 1, 0})));
    EXPECT_TRUE(set.count(Canon({0, 1, 0}, {0, 0, 0})));
}

TEST(EdgeExtractorTest, CoplanarQuad_DropsInteriorDiagonal) {
    std::vector<glm::vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    };
    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
    auto edges = ExtractFeatureEdges(positions, indices, {});

    auto set = ToSet(positions, edges);
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

    auto edges = ExtractFeatureEdges(positions, indices, {});
    auto set = ToSet(positions, edges);

    EXPECT_EQ(set.size(), 12u);
    EXPECT_TRUE(set.count(Canon({0, 0, 0}, {1, 0, 0})));
    EXPECT_TRUE(set.count(Canon({1, 0, 0}, {1, 1, 0})));
    EXPECT_TRUE(set.count(Canon({1, 1, 0}, {0, 1, 0})));
    EXPECT_TRUE(set.count(Canon({0, 1, 0}, {0, 0, 0})));
    EXPECT_FALSE(set.count(Canon({0, 0, 0}, {1, 1, 0})));
    EXPECT_FALSE(set.count(Canon({0, 0, 1}, {1, 1, 1})));
}

TEST(EdgeExtractorTest, FlatShadedCube_WeldsAndYieldsTwelveEdges) {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeFlatShadedCube(positions, indices);
    ASSERT_EQ(positions.size(), 36u);

    auto edges = ExtractFeatureEdges(positions, indices, {});
    auto set = ToSet(positions, edges);

    EXPECT_EQ(set.size(), 12u);
    // Emitted indices must reference the INPUT positions array.
    for (auto idx : edges) {
        EXPECT_LT(idx, positions.size());
    }
}

TEST(EdgeExtractorTest, HighThreshold_DropsShallowEdges) {
    const float angleDeg = 10.0f;
    const float h = std::tan(glm::radians(angleDeg));

    std::vector<glm::vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {0, 1, 0},
        {-1, 0, h}, {-1, 1, h},
    };
    std::vector<uint32_t> indices = {
        0, 1, 2,
        0, 2, 4,
        0, 4, 3,
    };

    EdgeExtractionConfig config;
    config.dihedralAngleDegrees = 30.0f;
    auto edges = ExtractFeatureEdges(positions, indices, config);
    auto set = ToSet(positions, edges);

    EXPECT_FALSE(set.count(Canon({0, 0, 0}, {0, 1, 0})));
}

TEST(EdgeExtractorTest, Deterministic_SameInputSameOutput) {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeSharedCube(positions, indices);

    auto a = ExtractFeatureEdges(positions, indices, {});
    auto b = ExtractFeatureEdges(positions, indices, {});

    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(EdgeExtractorTest, OutputIndicesReferenceInputPositions) {
    // Key contract: emitted indices are valid subscripts into the input
    // `positions` array, so callers can reuse the same vertex buffer for
    // triangle and edge draws.
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    MakeSharedCube(positions, indices);

    auto edges = ExtractFeatureEdges(positions, indices, {});
    ASSERT_FALSE(edges.empty());
    for (auto idx : edges) {
        ASSERT_LT(idx, positions.size());
    }
}
