#include "scene/EdgeExtractor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <glm/geometric.hpp>

namespace bimeup::scene {

namespace {

// Bucket positions on a uniform integer lattice sized by `weldEpsilon`, then
// union vertices whose buckets agree after rounding. A 27-cell neighbourhood
// scan catches positions straddling bucket boundaries. Also records, for
// each welded vertex, the first input index that mapped to it — so the
// extractor can emit output indices in the *input* positions space.
struct VertexWelder {
    explicit VertexWelder(float epsilon) : epsilon_(std::max(epsilon, 1e-12f)) {}

    void Weld(const std::vector<glm::vec3>& positions,
              std::vector<glm::vec3>& outUnique,
              std::vector<uint32_t>& outInputToWelded,
              std::vector<uint32_t>& outWeldedToFirstInput) {
        outUnique.clear();
        outInputToWelded.assign(positions.size(), UINT32_MAX);
        outWeldedToFirstInput.clear();

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
                outWeldedToFirstInput.push_back(i);
                grid[Key(bx, by, bz)].push_back(found);
            }
            outInputToWelded[i] = found;
        }
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

std::vector<uint32_t> ExtractFeatureEdges(const std::vector<glm::vec3>& positions,
                                          const std::vector<uint32_t>& indices,
                                          const EdgeExtractionConfig& config) {
    if (positions.empty() || indices.size() < 3) return {};

    // 1. Weld positions — topological comparison has to happen on unique
    //    vertices — and remember a representative input index per welded
    //    vertex so output indices land in input-positions space.
    std::vector<glm::vec3> welded;
    std::vector<uint32_t> inputToWelded;
    std::vector<uint32_t> weldedToFirstInput;
    VertexWelder welder(config.weldEpsilon);
    welder.Weld(positions, welded, inputToWelded, weldedToFirstInput);

    // 2. Walk triangles, compute normals, accumulate edge adjacency.
    std::unordered_map<EdgeKey, EdgeAdjacency, EdgeKeyHash> adjacency;
    adjacency.reserve(indices.size());

    const size_t triCount = indices.size() / 3;
    for (size_t t = 0; t < triCount; ++t) {
        const uint32_t i0 = inputToWelded[indices[3 * t + 0]];
        const uint32_t i1 = inputToWelded[indices[3 * t + 1]];
        const uint32_t i2 = inputToWelded[indices[3 * t + 2]];
        if (i0 == i1 || i1 == i2 || i0 == i2) continue;  // degenerate

        const glm::vec3& p0 = welded[i0];
        const glm::vec3& p1 = welded[i1];
        const glm::vec3& p2 = welded[i2];
        const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);
        const float area = glm::length(cross);
        if (area < 1e-12f) continue;
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

    // 3. Classify. Keep boundary / non-manifold / dihedral >= threshold.
    const float cosThreshold = std::cos(glm::radians(config.dihedralAngleDegrees));

    std::vector<EdgeKey> kept;
    kept.reserve(adjacency.size());
    for (const auto& [edge, adj] : adjacency) {
        if (adj.count == 1 || adj.nonManifold) {
            kept.push_back(edge);
            continue;
        }
        const float d = glm::dot(adj.normalA, adj.normalB);
        // Clamp guards against fp overshoot; a tiny negative overshoot
        // would flip the comparison otherwise.
        const float clamped = std::clamp(d, -1.0f, 1.0f);
        if (clamped <= cosThreshold) {
            kept.push_back(edge);
        }
    }
    // Sort so the output is independent of unordered_map iteration order.
    std::sort(kept.begin(), kept.end(), [](const EdgeKey& a, const EdgeKey& b) {
        return std::tuple{a.a, a.b} < std::tuple{b.a, b.b};
    });

    // 4. Emit indices in input-positions space, picking a representative
    //    input index per welded vertex.
    std::vector<uint32_t> out;
    out.reserve(kept.size() * 2);
    for (const EdgeKey& e : kept) {
        out.push_back(weldedToFirstInput[e.a]);
        out.push_back(weldedToFirstInput[e.b]);
    }
    return out;
}

}  // namespace bimeup::scene
