#include "scene/Slicing.h"

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

}  // namespace bimeup::scene
