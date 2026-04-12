#include <scene/SceneBuilder.h>
#include <scene/AABB.h>
#include <scene/SceneNode.h>

#include <ifc/IfcGeometryExtractor.h>
#include <ifc/IfcHierarchy.h>
#include <ifc/IfcModel.h>

#include <unordered_map>

namespace bimeup::scene {

namespace {

SceneMesh ConvertMesh(const ifc::TriangulatedMesh& src) {
    SceneMesh mesh;

    std::vector<glm::vec3> positions;
    positions.reserve(src.positions.size());
    for (const auto& p : src.positions) {
        positions.emplace_back(static_cast<float>(p.x),
                               static_cast<float>(p.y),
                               static_cast<float>(p.z));
    }

    std::vector<glm::vec3> normals;
    normals.reserve(src.normals.size());
    for (const auto& n : src.normals) {
        normals.emplace_back(static_cast<float>(n.x),
                             static_cast<float>(n.y),
                             static_cast<float>(n.z));
    }

    glm::vec4 color(static_cast<float>(src.color.r),
                    static_cast<float>(src.color.g),
                    static_cast<float>(src.color.b),
                    static_cast<float>(src.color.a));

    // Apply transformation to positions and normals
    glm::mat4 xform(src.transformation);
    glm::mat3 normalMatrix(glm::transpose(glm::inverse(glm::mat3(xform))));

    for (auto& p : positions) {
        p = glm::vec3(xform * glm::vec4(p, 1.0f));
    }
    for (auto& n : normals) {
        n = glm::normalize(normalMatrix * n);
    }

    mesh.SetPositions(std::move(positions));
    mesh.SetNormals(std::move(normals));
    mesh.SetIndices(src.indices);
    mesh.SetUniformColor(color);

    return mesh;
}

void BuildHierarchy(Scene& scene,
                    std::vector<SceneMesh>& meshes,
                    const ifc::HierarchyNode& hNode,
                    NodeId parentId,
                    const std::unordered_map<uint32_t, ifc::TriangulatedMesh>& meshMap) {
    SceneNode sNode;
    sNode.name = hNode.name;
    sNode.ifcType = hNode.type;
    sNode.globalId = hNode.globalId;
    sNode.parent = parentId;

    // If this element has geometry, convert and store the mesh
    auto it = meshMap.find(hNode.expressId);
    if (it != meshMap.end()) {
        MeshHandle handle = static_cast<MeshHandle>(meshes.size());
        SceneMesh converted = ConvertMesh(it->second);

        sNode.bounds = AABB::FromVertices(converted.GetPositions());
        sNode.mesh = handle;

        meshes.push_back(std::move(converted));
    }

    NodeId nodeId = scene.AddNode(std::move(sNode));

    for (const auto& child : hNode.children) {
        BuildHierarchy(scene, meshes, child, nodeId, meshMap);
    }
}

} // namespace

BuildResult SceneBuilder::Build(ifc::IfcModel& model) {
    BuildResult result;

    // Extract all geometry
    ifc::IfcGeometryExtractor extractor(model);
    auto allMeshes = extractor.ExtractAll();

    // Build lookup map: expressId → mesh
    std::unordered_map<uint32_t, ifc::TriangulatedMesh> meshMap;
    for (auto& [id, mesh] : allMeshes) {
        meshMap.emplace(id, std::move(mesh));
    }

    // Build scene from spatial hierarchy
    ifc::IfcHierarchy hierarchy(model);
    const auto& root = hierarchy.GetRoot();

    // Process root's children (Site, Building, etc.)
    // The root itself is the IfcProject — add it as the scene root
    BuildHierarchy(result.scene, result.meshes, root, InvalidNodeId, meshMap);

    return result;
}

} // namespace bimeup::scene
