#include <scene/SceneBuilder.h>
#include <scene/AABB.h>
#include <scene/SceneNode.h>

#include <ifc/IfcGeometryExtractor.h>
#include <ifc/IfcHierarchy.h>
#include <ifc/IfcModel.h>

#include <unordered_map>
#include <unordered_set>

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
    sNode.expressId = hNode.expressId;
    sNode.name = hNode.name;
    sNode.ifcType = hNode.type;
    sNode.globalId = hNode.globalId;
    sNode.parent = parentId;

    auto it = meshMap.find(hNode.expressId);
    std::optional<SceneMesh> convertedMesh;
    if (it != meshMap.end()) {
        MeshHandle handle = static_cast<MeshHandle>(meshes.size());
        convertedMesh = ConvertMesh(it->second);
        sNode.bounds = AABB::FromVertices(convertedMesh->GetPositions());
        sNode.mesh = handle;
    }

    NodeId nodeId = scene.AddNode(std::move(sNode));

    // Tag every triangle of this mesh with its owning NodeId so batching can
    // carry per-triangle ownership through the merge step.
    if (convertedMesh.has_value()) {
        size_t triangleCount = convertedMesh->GetIndices().size() / 3;
        std::vector<NodeId> owners(triangleCount, nodeId);
        convertedMesh->SetTriangleOwners(std::move(owners));
        meshes.push_back(std::move(*convertedMesh));
    }

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

    if (batchingEnabled_) {
        ApplyBatching(result, batchThreshold_);
    }

    return result;
}

// -- Batching implementation --------------------------------------------------

namespace {

struct BatchKey {
    std::string ifcType;
    uint32_t colorHash;

    bool operator==(const BatchKey& other) const {
        return ifcType == other.ifcType && colorHash == other.colorHash;
    }
};

struct BatchKeyHash {
    size_t operator()(const BatchKey& k) const {
        size_t h = std::hash<std::string>{}(k.ifcType);
        h ^= std::hash<uint32_t>{}(k.colorHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

uint32_t QuantizeColor(const glm::vec4& c) {
    auto r = static_cast<uint8_t>(c.r * 255.0f);
    auto g = static_cast<uint8_t>(c.g * 255.0f);
    auto b = static_cast<uint8_t>(c.b * 255.0f);
    auto a = static_cast<uint8_t>(c.a * 255.0f);
    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
}

SceneMesh MergeMeshes(const std::vector<const SceneMesh*>& meshes) {
    SceneMesh merged;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> colors;
    std::vector<uint32_t> indices;
    std::vector<NodeId> triangleOwners;

    uint32_t vertexOffset = 0;
    for (const auto* m : meshes) {
        const auto& mPos = m->GetPositions();
        const auto& mNrm = m->GetNormals();
        const auto& mCol = m->GetColors();
        const auto& mIdx = m->GetIndices();
        const auto& mOwn = m->GetTriangleOwners();

        positions.insert(positions.end(), mPos.begin(), mPos.end());
        normals.insert(normals.end(), mNrm.begin(), mNrm.end());
        colors.insert(colors.end(), mCol.begin(), mCol.end());
        for (auto idx : mIdx) {
            indices.push_back(idx + vertexOffset);
        }
        triangleOwners.insert(triangleOwners.end(), mOwn.begin(), mOwn.end());
        vertexOffset += static_cast<uint32_t>(m->GetVertexCount());
    }

    merged.SetPositions(std::move(positions));
    merged.SetNormals(std::move(normals));
    merged.SetColors(std::move(colors));
    merged.SetIndices(std::move(indices));
    merged.SetTriangleOwners(std::move(triangleOwners));

    return merged;
}

} // namespace

void SceneBuilder::ApplyBatching(BuildResult& result, size_t maxVertices) {
    struct NodeMeshInfo {
        NodeId nodeId;
        MeshHandle meshHandle;
        BatchKey key;
    };

    std::vector<NodeMeshInfo> smallItems;
    std::unordered_set<MeshHandle> largeHandleSet;
    std::vector<MeshHandle> largeHandles;

    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        auto nodeId = static_cast<NodeId>(i);
        const auto& node = result.scene.GetNode(nodeId);
        if (!node.mesh.has_value()) continue;

        MeshHandle handle = node.mesh.value();
        const auto& mesh = result.meshes[handle];

        if (mesh.GetVertexCount() > maxVertices) {
            if (largeHandleSet.insert(handle).second) {
                largeHandles.push_back(handle);
            }
        } else {
            BatchKey key;
            key.ifcType = node.ifcType;
            key.colorHash = mesh.GetColors().empty()
                                ? 0
                                : QuantizeColor(mesh.GetColors()[0]);
            smallItems.push_back({nodeId, handle, key});
        }
    }

    if (smallItems.empty()) return;

    // Group small meshes by batch key
    std::unordered_map<BatchKey, std::vector<size_t>, BatchKeyHash> groups;
    for (size_t i = 0; i < smallItems.size(); ++i) {
        groups[smallItems[i].key].push_back(i);
    }

    // Build new mesh list: large meshes first, then one merged mesh per group
    std::vector<SceneMesh> newMeshes;

    // Large meshes keep their own slots
    std::unordered_map<MeshHandle, MeshHandle> largeRemap;
    for (auto oldHandle : largeHandles) {
        auto newHandle = static_cast<MeshHandle>(newMeshes.size());
        newMeshes.push_back(std::move(result.meshes[oldHandle]));
        largeRemap[oldHandle] = newHandle;
    }

    // Merge each small-mesh group
    std::unordered_map<NodeId, MeshHandle> nodeToNewHandle;
    for (auto& [key, memberIndices] : groups) {
        std::vector<const SceneMesh*> toMerge;
        toMerge.reserve(memberIndices.size());
        for (auto idx : memberIndices) {
            toMerge.push_back(&result.meshes[smallItems[idx].meshHandle]);
        }

        auto newHandle = static_cast<MeshHandle>(newMeshes.size());
        newMeshes.push_back(MergeMeshes(toMerge));

        for (auto idx : memberIndices) {
            nodeToNewHandle[smallItems[idx].nodeId] = newHandle;
        }
    }

    // Update node mesh handles
    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        auto nodeId = static_cast<NodeId>(i);
        auto& node = result.scene.GetNode(nodeId);
        if (!node.mesh.has_value()) continue;

        auto it = nodeToNewHandle.find(nodeId);
        if (it != nodeToNewHandle.end()) {
            node.mesh = it->second;
        } else {
            auto lit = largeRemap.find(node.mesh.value());
            if (lit != largeRemap.end()) {
                node.mesh = lit->second;
            }
        }
    }

    result.meshes = std::move(newMeshes);
}

} // namespace bimeup::scene
