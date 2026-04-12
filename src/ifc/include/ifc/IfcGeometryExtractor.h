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

private:
    IfcModel& model_;
};

} // namespace bimeup::ifc
