#include "scene/EdgeExtractor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <glm/geometric.hpp>

namespace bimeup::scene {

namespace {

// Bucket positions on a uniform integer lattice sized by `weldEpsilon`, then
// union vertices whose buckets agree after rounding. A 27-cell neighbourhood
// scan catches positions straddling bucket boundaries.
struct VertexWelder {
    explicit VertexWelder(float epsilon) : epsilon_(std::max(epsilon, 1e-12f)) {}

    uint32_t Weld(const std::vector<glm::vec3>& positions,
                  std::vector<glm::vec3>& outUnique,
                  std::vector<uint32_t>& outRemap) {
        outUnique.clear();
        outRemap.assign(positions.size(), UINT32_MAX);

        const float invEps = 1.0f / epsilon_;
        std::unordered_map<std::uint64_t, std::vector<uint32_t>> grid;
        grid.reserve(positions.size());

        const float eps2 = epsilon_ * epsilon_;

        for (uint32_t i = 0; i < positions.size(); ++i) {
            const glm::vec3& p = positions[i];
            const std::int32_t bx = static_cast<std::int32_t>(std::floor(p.x * invEps));
            const std::int32_t by = static_cast<std::int32_t>(std::floor(p.y * invEps));
            const std::int32_t bz = static_cast<std::int32_t>(std::floor(p.z * invEps));

            uint32_t found = UINT32_MAX;
            for (int dz = -1; dz <= 1 && found == UINT32_MAX; ++dz) {
                for (int dy = -1; dy <= 1 && found == UINT32_MAX; ++dy) {
                    for (int dx = -1; dx <= 1 && found == UINT32_MAX; ++dx) {
                        const auto it = grid.find(Key(bx + dx, by + dy, bz + dz));
                        if (it == grid.end()) continue;
                        for (uint32_t cand : it->second) {
                            const glm::vec3 d = outUnique[cand] - p;
                            if (glm::dot(d, d) <= eps2) {
                                found = cand;
                                break;
                            }
                        }
                    }
                }
            }

            if (found == UINT32_MAX) {
                found = static_cast<uint32_t>(outUnique.size());
                outUnique.push_back(p);
                grid[Key(bx, by, bz)].push_back(found);
            }
            outRemap[i] = found;
        }

        return static_cast<uint32_t>(outUnique.size());
    }

private:
    static std::uint64_t Key(std::int32_t x, std::int32_t y, std::int32_t z) {
        auto enc = [](std::int32_t v) {
            return static_cast<std::uint64_t>(static_cast<std::uint32_t>(v));
        };
        return (enc(x) * 0x9E3779B97F4A7C15ull) ^
               (enc(y) * 0xBF58476D1CE4E5B9ull) ^
               (enc(z) * 0x94D049BB133111EBull);
    }

    float epsilon_;
};

struct EdgeKey {
    uint32_t a;
    uint32_t b;

    bool operator==(const EdgeKey& other) const { return a == other.a && b == other.b; }
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& e) const noexcept {
        return (static_cast<std::size_t>(e.a) * 0x9E3779B97F4A7C15ull) ^
               static_cast<std::size_t>(e.b);
    }
};

EdgeKey MakeEdge(uint32_t u, uint32_t v) {
    return u < v ? EdgeKey{u, v} : EdgeKey{v, u};
}

// Adjacency entry: up to two triangle normals per edge. A third incident
// triangle (non-manifold) forces the edge to be treated as a feature edge.
struct EdgeAdjacency {
    glm::vec3 normalA{0.0f};
    glm::vec3 normalB{0.0f};
    uint32_t count = 0;
    bool nonManifold = false;
};

}  // namespace

ExtractedEdges ExtractFeatureEdges(const std::vector<glm::vec3>& positions,
                                   const std::vector<uint32_t>& indices,
                                   const EdgeExtractionConfig& config) {
    ExtractedEdges out;
    if (positions.empty() || indices.size() < 3) return out;

    // 1. Weld positions — topological comparison has to happen on unique verts.
    std::vector<glm::vec3> welded;
    std::vector<uint32_t> remap;
    VertexWelder welder(config.weldEpsilon);
    welder.Weld(positions, welded, remap);

    // 2. Walk triangles, compute normals, accumulate edge adjacency.
    std::unordered_map<EdgeKey, EdgeAdjacency, EdgeKeyHash> adjacency;
    adjacency.reserve(indices.size());  // upper bound ~= 3 * triCount

    const size_t triCount = indices.size() / 3;
    for (size_t t = 0; t < triCount; ++t) {
        const uint32_t i0 = remap[indices[3 * t + 0]];
        const uint32_t i1 = remap[indices[3 * t + 1]];
        const uint32_t i2 = remap[indices[3 * t + 2]];
        if (i0 == i1 || i1 == i2 || i0 == i2) continue;  // degenerate

        const glm::vec3& p0 = welded[i0];
        const glm::vec3& p1 = welded[i1];
        const glm::vec3& p2 = welded[i2];
        const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);
        const float area = glm::length(cross);
        if (area < 1e-12f) continue;  // zero-area triangle
        const glm::vec3 normal = cross / area;

        const EdgeKey edges[3] = {
            MakeEdge(i0, i1),
            MakeEdge(i1, i2),
            MakeEdge(i2, i0),
        };
        for (const EdgeKey& e : edges) {
            EdgeAdjacency& adj = adjacency[e];
            if (adj.count == 0) {
                adj.normalA = normal;
            } else if (adj.count == 1) {
                adj.normalB = normal;
            } else {
                adj.nonManifold = true;
            }
            ++adj.count;
        }
    }

    // 3. Classify edges. Keep boundary (count==1), non-manifold, or dihedral
    // >= threshold. Ordering of the output follows a sorted sweep so
    // determinism does not depend on unordered_map iteration.
    const float cosThreshold = std::cos(glm::radians(config.dihedralAngleDegrees));

    std::vector<EdgeKey> kept;
    kept.reserve(adjacency.size());
    for (const auto& [edge, adj] : adjacency) {
        if (adj.count == 1 || adj.nonManifold) {
            kept.push_back(edge);
            continue;
        }
        // count == 2: compare normals. Dihedral angle = acos(dot).
        const float d = glm::dot(adj.normalA, adj.normalB);
        // Clamp guards against fp overshoot; a small negative overshoot
        // would flip the comparison otherwise.
        const float clamped = std::clamp(d, -1.0f, 1.0f);
        if (clamped <= cosThreshold) {
            kept.push_back(edge);
        }
    }
    std::sort(kept.begin(), kept.end(), [](const EdgeKey& a, const EdgeKey& b) {
        return std::tuple{a.a, a.b} < std::tuple{b.a, b.b};
    });

    // 4. Emit. Only welded positions that participate in a kept edge are
    // copied into the output, with indices remapped to the compact range.
    std::vector<uint32_t> compact(welded.size(), UINT32_MAX);
    out.indices.reserve(kept.size() * 2);
    for (const EdgeKey& e : kept) {
        for (uint32_t v : {e.a, e.b}) {
            if (compact[v] == UINT32_MAX) {
                compact[v] = static_cast<uint32_t>(out.positions.size());
                out.positions.push_back(welded[v]);
            }
            out.indices.push_back(compact[v]);
        }
    }

    return out;
}

}  // namespace bimeup::scene
