#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::ifc {

class IfcModel;

struct TriangulatedMesh {
    std::vector<glm::dvec3> positions;
    std::vector<glm::dvec3> normals;
    std::vector<uint32_t> indices;
    glm::dvec4 color{1.0, 1.0, 1.0, 1.0};
    glm::dmat4 transformation{1.0};
};

class IfcGeometryExtractor {
public:
    explicit IfcGeometryExtractor(IfcModel& model);

    std::optional<TriangulatedMesh> ExtractMesh(uint32_t expressId) const;
    std::vector<std::pair<uint32_t, TriangulatedMesh>> ExtractAll() const;

    /// Extract one `TriangulatedMesh` per IfcPlacedGeometry belonging to the
    /// element. Each sub-mesh keeps its own surface-style color (including
    /// alpha < 1 for translucent materials like glass panes) and its own
    /// transformation. Returns an empty vector if the element has no geometry
    /// or the id is unknown.
    std::vector<TriangulatedMesh> ExtractSubMeshes(uint32_t expressId) const;

private:
    IfcModel& model_;
};

} // namespace bimeup::ifc
