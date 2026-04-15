#include "ifc/IfcGeometryExtractor.h"
#include "ifc/IfcModel.h"

#include <web-ifc/geometry/IfcGeometryProcessor.h>
#include <web-ifc/geometry/representation/IfcGeometry.h>
#include <web-ifc/geometry/representation/geometry.h>

namespace bimeup::ifc {

IfcGeometryExtractor::IfcGeometryExtractor(IfcModel& model)
    : model_(model) {}

std::optional<TriangulatedMesh> IfcGeometryExtractor::ExtractMesh(uint32_t expressId) const {
    auto* processor = model_.GetGeometryProcessor();
    if (!processor) {
        return std::nullopt;
    }

    webifc::geometry::IfcFlatMesh flatMesh;
    try {
        flatMesh = processor->GetFlatMesh(expressId);
    } catch (...) {
        return std::nullopt;
    }

    if (flatMesh.geometries.empty()) {
        return std::nullopt;
    }

    // Merge all placed geometries into one TriangulatedMesh.
    // Use the first placed geometry's color and transformation as the element's.
    TriangulatedMesh result;
    result.color = flatMesh.geometries[0].color;
    result.transformation = flatMesh.geometries[0].transformation;

    for (const auto& placedGeom : flatMesh.geometries) {
        auto& geom = processor->GetGeometry(placedGeom.geometryExpressID);

        size_t vertexCount = geom.numPoints;
        if (vertexCount == 0) {
            continue;
        }

        uint32_t baseIndex = static_cast<uint32_t>(result.positions.size());

        // Extract positions and normals from interleaved vertex data
        // Format: [x, y, z, nx, ny, nz] per vertex (6 doubles each)
        constexpr int kStride = 6;
        for (size_t v = 0; v < vertexCount; ++v) {
            size_t offset = v * kStride;
            glm::dvec3 pos(geom.vertexData[offset + 0],
                           geom.vertexData[offset + 1],
                           geom.vertexData[offset + 2]);
            glm::dvec3 normal(geom.vertexData[offset + 3],
                              geom.vertexData[offset + 4],
                              geom.vertexData[offset + 5]);

            result.positions.push_back(pos);
            result.normals.push_back(normal);
        }

        // Rebase indices
        for (uint32_t idx : geom.indexData) {
            result.indices.push_back(baseIndex + idx);
        }
    }

    if (result.positions.empty()) {
        return std::nullopt;
    }

    return result;
}

std::vector<TriangulatedMesh> IfcGeometryExtractor::ExtractSubMeshes(uint32_t expressId) const {
    auto* processor = model_.GetGeometryProcessor();
    if (!processor) {
        return {};
    }

    webifc::geometry::IfcFlatMesh flatMesh;
    try {
        flatMesh = processor->GetFlatMesh(expressId);
    } catch (...) {
        return {};
    }

    if (flatMesh.geometries.empty()) {
        return {};
    }

    std::vector<TriangulatedMesh> subs;
    subs.reserve(flatMesh.geometries.size());

    for (const auto& placedGeom : flatMesh.geometries) {
        auto& geom = processor->GetGeometry(placedGeom.geometryExpressID);
        size_t vertexCount = geom.numPoints;
        if (vertexCount == 0) {
            continue;
        }

        TriangulatedMesh sub;
        sub.color = placedGeom.color;
        sub.transformation = placedGeom.transformation;
        sub.positions.reserve(vertexCount);
        sub.normals.reserve(vertexCount);

        // Interleaved: [x, y, z, nx, ny, nz] per vertex (6 doubles).
        constexpr int kStride = 6;
        for (size_t v = 0; v < vertexCount; ++v) {
            size_t offset = v * kStride;
            sub.positions.emplace_back(geom.vertexData[offset + 0],
                                       geom.vertexData[offset + 1],
                                       geom.vertexData[offset + 2]);
            sub.normals.emplace_back(geom.vertexData[offset + 3],
                                     geom.vertexData[offset + 4],
                                     geom.vertexData[offset + 5]);
        }
        sub.indices = geom.indexData;

        subs.push_back(std::move(sub));
    }

    return subs;
}

std::vector<std::pair<uint32_t, TriangulatedMesh>> IfcGeometryExtractor::ExtractAll() const {
    std::vector<std::pair<uint32_t, TriangulatedMesh>> results;

    auto ids = model_.GetElementExpressIds();
    for (uint32_t id : ids) {
        auto mesh = ExtractMesh(id);
        if (mesh.has_value()) {
            results.emplace_back(id, std::move(*mesh));
        }
    }

    return results;
}

} // namespace bimeup::ifc
