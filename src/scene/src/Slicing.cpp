#include "scene/Slicing.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "scene/SceneMesh.h"

namespace bimeup::scene {

namespace {

int Count(renderer::PointSide s,
          renderer::PointSide sa,
          renderer::PointSide sb,
          renderer::PointSide sc) {
    return (sa == s ? 1 : 0) + (sb == s ? 1 : 0) + (sc == s ? 1 : 0);
}

glm::vec3 Interpolate(const renderer::ClipPlane& plane,
                      const glm::vec3& p,
                      const glm::vec3& q) {
    const float sp = renderer::SignedDistance(plane, p);
    const float sq = renderer::SignedDistance(plane, q);
    const float t = sp / (sp - sq);
    return p + t * (q - p);
}

}  // namespace

TriangleCut SliceTriangle(const renderer::ClipPlane& plane,
                          const glm::vec3& a,
                          const glm::vec3& b,
                          const glm::vec3& c) {
    using renderer::PointSide;

    const PointSide sa = renderer::ClassifyPoint(plane, a);
    const PointSide sb = renderer::ClassifyPoint(plane, b);
    const PointSide sc = renderer::ClassifyPoint(plane, c);

    const int nFront = Count(PointSide::Front, sa, sb, sc);
    const int nBack = Count(PointSide::Back, sa, sb, sc);
    const int nOn = Count(PointSide::OnPlane, sa, sb, sc);

    TriangleCut cut{};

    // No straddle: emit a segment only for the edge-on-plane case (2 verts on
    // plane, third on one side). All other non-straddling configurations
    // (all-same-side, coplanar, single-vertex touch) drop.
    if (nFront == 0 || nBack == 0) {
        if (nOn == 2 && (nFront + nBack) == 1) {
            std::array<glm::vec3, 2> pts{};
            int k = 0;
            if (sa == PointSide::OnPlane) pts[k++] = a;
            if (sb == PointSide::OnPlane) pts[k++] = b;
            if (sc == PointSide::OnPlane) pts[k++] = c;
            cut.pointCount = 2;
            cut.points = pts;
        }
        return cut;
    }

    // Straddle: at least one Front and one Back vertex. Walk the three edges,
    // emitting an on-plane vertex once (when its edge leaves the plane) and an
    // interpolated point for each Front/Back crossing.
    std::array<glm::vec3, 2> pts{};
    int k = 0;

    auto processEdge = [&](const glm::vec3& p, const glm::vec3& q,
                           PointSide ps, PointSide qs) {
        if (k >= 2) return;
        if (ps == PointSide::OnPlane) {
            if (qs != PointSide::OnPlane) pts[k++] = p;
            return;
        }
        if (qs == PointSide::OnPlane) {
            // q is emitted when the next edge (starting at q) is processed.
            return;
        }
        if (ps != qs) {
            pts[k++] = Interpolate(plane, p, q);
        }
    };

    processEdge(a, b, sa, sb);
    processEdge(b, c, sb, sc);
    processEdge(c, a, sc, sa);

    if (k == 2) {
        cut.pointCount = 2;
        cut.points = pts;
    }
    return cut;
}

std::vector<Segment> SliceSceneMesh(const SceneMesh& mesh,
                                    const glm::mat4& worldTransform,
                                    const renderer::ClipPlane& plane) {
    std::vector<Segment> segments;
    const auto& positions = mesh.GetPositions();
    const auto& indices = mesh.GetIndices();
    const size_t triCount = indices.size() / 3;
    segments.reserve(triCount / 4);

    for (size_t t = 0; t < triCount; ++t) {
        const uint32_t ia = indices[3 * t + 0];
        const uint32_t ib = indices[3 * t + 1];
        const uint32_t ic = indices[3 * t + 2];
        if (ia >= positions.size() || ib >= positions.size() ||
            ic >= positions.size()) {
            continue;
        }
        const glm::vec3 a = glm::vec3(worldTransform * glm::vec4(positions[ia], 1.0F));
        const glm::vec3 b = glm::vec3(worldTransform * glm::vec4(positions[ib], 1.0F));
        const glm::vec3 c = glm::vec3(worldTransform * glm::vec4(positions[ic], 1.0F));

        const TriangleCut cut = SliceTriangle(plane, a, b, c);
        if (cut.pointCount == 2) {
            segments.push_back({cut.points[0], cut.points[1]});
        }
    }
    return segments;
}

namespace {

struct GridKey {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
    bool operator==(const GridKey& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct GridKeyHash {
    std::size_t operator()(const GridKey& k) const noexcept {
        // FNV-ish mixing of three int64s.
        std::size_t h = std::hash<std::int64_t>{}(k.x);
        h ^= std::hash<std::int64_t>{}(k.y) + 0x9e3779b97f4a7c15ULL +
             (h << 6) + (h >> 2);
        h ^= std::hash<std::int64_t>{}(k.z) + 0x9e3779b97f4a7c15ULL +
             (h << 6) + (h >> 2);
        return h;
    }
};

GridKey Quantise(const glm::vec3& p, float epsilon) {
    const float inv = 1.0F / epsilon;
    return GridKey{static_cast<std::int64_t>(std::llround(p.x * inv)),
                   static_cast<std::int64_t>(std::llround(p.y * inv)),
                   static_cast<std::int64_t>(std::llround(p.z * inv))};
}

}  // namespace

std::vector<std::vector<glm::vec3>> StitchSegments(
    std::span<const Segment> segments, float epsilon) {
    std::vector<std::vector<glm::vec3>> polygons;
    if (segments.empty()) return polygons;

    // Build adjacency: for each quantised endpoint, list (segmentIndex, isB)
    // — isB means this endpoint is the segment's B end (so its "other" end is A).
    struct EndpointRef {
        std::size_t segIndex;
        bool isB;
    };
    std::unordered_map<GridKey, std::vector<EndpointRef>, GridKeyHash> grid;
    grid.reserve(segments.size() * 2);

    for (std::size_t i = 0; i < segments.size(); ++i) {
        grid[Quantise(segments[i].a, epsilon)].push_back({i, false});
        grid[Quantise(segments[i].b, epsilon)].push_back({i, true});
    }

    std::vector<bool> used(segments.size(), false);

    for (std::size_t start = 0; start < segments.size(); ++start) {
        if (used[start]) continue;

        std::vector<glm::vec3> poly;
        poly.push_back(segments[start].a);

        const GridKey startKey = Quantise(segments[start].a, epsilon);
        std::size_t current = start;
        bool currentIsB = false;  // current endpoint we're walking from is .b
        used[current] = true;
        glm::vec3 currentEnd = segments[current].b;

        bool closed = false;
        while (true) {
            const GridKey endKey = Quantise(currentEnd, epsilon);
            if (endKey == startKey) {
                closed = true;
                break;
            }
            poly.push_back(currentEnd);

            // Find an unused neighbour segment sharing this endpoint.
            auto it = grid.find(endKey);
            if (it == grid.end()) break;
            std::size_t next = segments.size();
            bool nextOtherIsB = false;
            for (const auto& ref : it->second) {
                if (ref.segIndex == current) continue;
                if (used[ref.segIndex]) continue;
                next = ref.segIndex;
                // ref.isB tells which end matched; the "other" end becomes
                // the new walking endpoint.
                nextOtherIsB = !ref.isB;
                break;
            }
            if (next == segments.size()) break;

            used[next] = true;
            current = next;
            currentIsB = nextOtherIsB;
            currentEnd = currentIsB ? segments[next].b : segments[next].a;
        }

        if (closed && poly.size() >= 3) {
            polygons.push_back(std::move(poly));
        }
    }

    return polygons;
}

}  // namespace bimeup::scene
