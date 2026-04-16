#include "scene/Slicing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <unordered_map>

#include <poly2tri/poly2tri.h>

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

StitchResult StitchSegmentsDetailed(std::span<const Segment> segments,
                                    float epsilon) {
    StitchResult result;
    if (segments.empty()) return result;

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

    // Match check: two endpoints are "same" if they quantize to the same cell
    // OR to any of the 26 neighbouring cells (3×3×3). The neighbour sweep is
    // the welding prepass that closes section fills on dense/noisy IFC
    // geometry (e.g. arched roofs) where endpoints straddling a cell boundary
    // otherwise fail to match.
    auto keysMatch = [](const GridKey& a, const GridKey& b) {
        return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1 &&
               std::abs(a.z - b.z) <= 1;
    };
    auto findNeighbourSegment = [&](const GridKey& key, std::size_t current,
                                    const std::vector<bool>& usedRef,
                                    std::size_t& outIdx, bool& outOtherIsB) {
        for (std::int64_t dx = -1; dx <= 1; ++dx)
            for (std::int64_t dy = -1; dy <= 1; ++dy)
                for (std::int64_t dz = -1; dz <= 1; ++dz) {
                    GridKey k{key.x + dx, key.y + dy, key.z + dz};
                    auto it = grid.find(k);
                    if (it == grid.end()) continue;
                    for (const auto& ref : it->second) {
                        if (ref.segIndex == current) continue;
                        if (usedRef[ref.segIndex]) continue;
                        outIdx = ref.segIndex;
                        outOtherIsB = !ref.isB;
                        return true;
                    }
                }
        return false;
    };

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
            if (keysMatch(endKey, startKey) && poly.size() >= 2) {
                closed = true;
                break;
            }
            poly.push_back(currentEnd);

            std::size_t next = segments.size();
            bool nextOtherIsB = false;
            if (!findNeighbourSegment(endKey, current, used, next,
                                      nextOtherIsB)) {
                break;
            }

            used[next] = true;
            current = next;
            currentIsB = nextOtherIsB;
            currentEnd = currentIsB ? segments[next].b : segments[next].a;
        }

        if (closed && poly.size() >= 3) {
            result.closed.push_back(std::move(poly));
        } else if (poly.size() >= 3) {
            result.open.push_back(std::move(poly));
        }
    }

    return result;
}

std::vector<std::vector<glm::vec3>> StitchSegments(
    std::span<const Segment> segments, float epsilon) {
    return StitchSegmentsDetailed(segments, epsilon).closed;
}

namespace {

// Project a 3D point onto 2D by dropping the axis with the largest
// |normal| component. Returns (u, v) and the index of the dropped axis.
int DominantAxis(const glm::vec3& n) {
    const glm::vec3 a = glm::abs(n);
    if (a.x >= a.y && a.x >= a.z) return 0;
    if (a.y >= a.z) return 1;
    return 2;
}

glm::vec2 Project(const glm::vec3& p, int drop) {
    switch (drop) {
        case 0: return {p.y, p.z};
        case 1: return {p.x, p.z};
        default: return {p.x, p.y};
    }
}

float SignedArea2D(const std::vector<glm::vec2>& p) {
    float s = 0.0F;
    const std::size_t n = p.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = p[i];
        const auto& b = p[(i + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5F * s;
}

float Cross2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool PointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b,
                     const glm::vec2& c) {
    const float d1 = Cross2D(p, a, b);
    const float d2 = Cross2D(p, b, c);
    const float d3 = Cross2D(p, c, a);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

}  // namespace

namespace {

// Ear-clip fallback used when poly2tri throws (degenerate / non-simple input).
std::vector<glm::vec3> EarClipTriangulate(const std::vector<glm::vec3>& polygon,
                                          const std::vector<glm::vec2>& verts2d) {
    std::vector<glm::vec3> result;
    const std::size_t n = polygon.size();
    if (n < 3) return result;

    std::vector<std::size_t> idx;
    idx.reserve(n);
    if (SignedArea2D(verts2d) >= 0.0F) {
        for (std::size_t i = 0; i < n; ++i) idx.push_back(i);
    } else {
        for (std::size_t i = 0; i < n; ++i) idx.push_back(n - 1 - i);
    }

    result.reserve(3 * (n - 2));

    std::size_t guard = 0;
    const std::size_t maxIters = n * n + 8;
    while (idx.size() >= 3 && guard++ < maxIters) {
        const std::size_t m = idx.size();
        bool earFound = false;
        for (std::size_t i = 0; i < m; ++i) {
            const std::size_t iPrev = idx[(i + m - 1) % m];
            const std::size_t iCur = idx[i];
            const std::size_t iNext = idx[(i + 1) % m];
            const glm::vec2& a = verts2d[iPrev];
            const glm::vec2& b = verts2d[iCur];
            const glm::vec2& c = verts2d[iNext];
            if (Cross2D(a, b, c) <= 0.0F) continue;

            bool contains = false;
            for (std::size_t j = 0; j < m; ++j) {
                if (j == (i + m - 1) % m || j == i || j == (i + 1) % m) continue;
                if (PointInTriangle(verts2d[idx[j]], a, b, c)) {
                    contains = true;
                    break;
                }
            }
            if (contains) continue;

            result.push_back(polygon[iPrev]);
            result.push_back(polygon[iCur]);
            result.push_back(polygon[iNext]);
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            earFound = true;
            break;
        }
        if (!earFound) break;
    }
    return result;
}

}  // namespace

std::vector<glm::vec3> TriangulatePolygon(const std::vector<glm::vec3>& polygon,
                                          const glm::vec3& planeNormal) {
    if (polygon.size() < 3) return {};

    const int drop = DominantAxis(planeNormal);

    // Project to 2D and dedupe near-duplicate consecutive points (poly2tri
    // rejects polylines with repeated vertices). Also drop a closing
    // duplicate if last == first.
    std::vector<glm::vec3> clean;
    std::vector<glm::vec2> clean2d;
    clean.reserve(polygon.size());
    clean2d.reserve(polygon.size());
    constexpr float kDedupEps2 = 1e-10F;
    for (const auto& p : polygon) {
        const glm::vec2 q = Project(p, drop);
        if (!clean2d.empty()) {
            const glm::vec2 d = q - clean2d.back();
            if (d.x * d.x + d.y * d.y < kDedupEps2) continue;
        }
        clean.push_back(p);
        clean2d.push_back(q);
    }
    if (clean.size() >= 2) {
        const glm::vec2 d = clean2d.front() - clean2d.back();
        if (d.x * d.x + d.y * d.y < kDedupEps2) {
            clean.pop_back();
            clean2d.pop_back();
        }
    }
    if (clean.size() < 3) return {};

    // poly2tri expects CCW input — reverse if our projected polygon is CW.
    const bool reversed = SignedArea2D(clean2d) < 0.0F;
    if (reversed) {
        std::reverse(clean.begin(), clean.end());
        std::reverse(clean2d.begin(), clean2d.end());
    }

    // Storage is a vector reserved once so pointers stay stable.
    std::vector<p2t::Point> storage;
    storage.reserve(clean.size());
    std::vector<p2t::Point*> polyline;
    polyline.reserve(clean.size());
    for (std::size_t i = 0; i < clean2d.size(); ++i) {
        storage.emplace_back(static_cast<double>(clean2d[i].x),
                             static_cast<double>(clean2d[i].y));
        polyline.push_back(&storage.back());
    }

    // Map p2t::Point* back to the original 3D vertex via its index in storage.
    auto indexOf = [&](p2t::Point* pt) -> std::size_t {
        const std::ptrdiff_t off = pt - storage.data();
        if (off < 0 || static_cast<std::size_t>(off) >= storage.size()) {
            return storage.size();
        }
        return static_cast<std::size_t>(off);
    };

    std::vector<glm::vec3> result;
    try {
        p2t::CDT cdt(polyline);
        cdt.Triangulate();
        const auto tris = cdt.GetTriangles();
        result.reserve(tris.size() * 3);
        for (auto* tri : tris) {
            bool bad = false;
            std::array<glm::vec3, 3> verts{};
            for (int k = 0; k < 3; ++k) {
                const std::size_t idx = indexOf(tri->GetPoint(k));
                if (idx >= clean.size()) { bad = true; break; }
                verts[k] = clean[idx];
            }
            if (bad) continue;
            result.push_back(verts[0]);
            result.push_back(verts[1]);
            result.push_back(verts[2]);
        }
    } catch (const std::exception&) {
        // Non-simple / degenerate input — fall back to ear-clip on the
        // original (non-reversed) projection so existing behaviour survives.
        const auto fallback2d = reversed ? std::vector<glm::vec2>(
                                               clean2d.rbegin(), clean2d.rend())
                                         : clean2d;
        const auto fallback3d = reversed ? std::vector<glm::vec3>(
                                               clean.rbegin(), clean.rend())
                                         : clean;
        return EarClipTriangulate(fallback3d, fallback2d);
    }
    return result;
}

}  // namespace bimeup::scene
