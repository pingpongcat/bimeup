#include "scene/Slicing.h"

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

}  // namespace bimeup::scene
