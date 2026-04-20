## Stage 4 — IFC Loading & Internal Scene Representation

**Goal**: Load an IFC file, convert it to an internal optimized scene representation, display the geometry in the viewer.

**Sessions**: 3–4 (this is a critical stage)

### Modules involved
- `ifc/` (IFC parsing via IfcOpenShell/web-ifc)
- `scene/` (internal representation)
- `renderer/` (mesh upload)
- `core/` (orchestration)

### Key design decisions

The `ifc/` module wraps an IFC parsing library and produces a **library-agnostic intermediate representation**. The `scene/` module converts this into a flat, GPU-friendly scene graph. The renderer only sees `scene/` types.

```
IFC file → ifc::IfcModel → scene::Scene → renderer (GPU buffers)
```

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 4.1 | Integrate IFC parsing library (IfcOpenShell or web-ifc C++ core) as submodule | Builds and links | `external/` |
| 4.2 | Create `ifc/IfcModel` — wraps parsed IFC data, exposes element iteration | Unit test: load test.ifc, iterate elements, read properties | `src/ifc/include/ifc/IfcModel.h` |
| 4.3 | Create `ifc/IfcElement` — single IFC element with type, name, GUID, properties | Unit test: element has correct type (IfcWall, IfcSlab…), name, globalId | `src/ifc/include/ifc/IfcElement.h` |
| 4.4 | Create `ifc/IfcGeometryExtractor` — extract triangulated mesh data per element | Unit test: extract geometry from IfcWall, verify vertex/index count > 0 | `src/ifc/include/ifc/IfcGeometryExtractor.h` |
| 4.5 | Create `ifc/IfcHierarchy` — extract spatial structure tree (Site→Building→Storey→Elements) | Unit test: hierarchy depth ≥ 3 for test file, element count matches | `src/ifc/include/ifc/IfcHierarchy.h` |
| 4.6 | Create `scene/SceneNode` and `scene/Scene` — flat scene graph with transforms | Unit test: create scene, add nodes, query by ID, parent-child relations | `src/scene/` |
| 4.7 | Create `scene/SceneMesh` — GPU-ready mesh data (positions, normals, colors, indices) | Unit test: create mesh, verify data layout matches renderer expectations | `src/scene/include/scene/SceneMesh.h` |
| 4.8 | Create `scene/AABB` — axis-aligned bounding box per node and for whole scene | Unit test: compute AABB from vertices, merge two AABBs | `src/scene/include/scene/AABB.h` |
| 4.9 | Create `scene/SceneBuilder` — converts `ifc::IfcModel` → `scene::Scene` | Integration test: load IFC → build scene → verify node count, mesh count | `src/scene/include/scene/SceneBuilder.h` |
| 4.10 | Implement batching in SceneBuilder — group small meshes by material/type | Unit test: 100 small elements produce fewer batched draw calls | Batching logic |
| 4.11 | Upload scene meshes to renderer, draw all elements | Visual test: IFC model visible in viewer | Integration in core/ |
| 4.12 | Implement per-element color from IFC material/style | Unit test: element with IfcSurfaceStyle gets correct color | Color extraction in ifc/ |

### Expected APIs after Stage 4

```cpp
// ifc/include/ifc/IfcModel.h
namespace bimeup::ifc {
    class IfcModel {
    public:
        bool LoadFromFile(const std::string& path);
        size_t GetElementCount() const;
        std::vector<IfcElement> GetElements() const;
        std::vector<IfcElement> GetElementsByType(const std::string& ifcType) const;
        IfcElement GetElementByGlobalId(const std::string& guid) const;
        IfcHierarchy GetSpatialHierarchy() const;
    };
}

// ifc/include/ifc/IfcGeometryExtractor.h
namespace bimeup::ifc {
    struct TriangulatedMesh {
        std::vector<glm::dvec3> positions;
        std::vector<glm::dvec3> normals;
        std::vector<uint32_t> indices;
        glm::dvec4 color;           // from IFC style
        glm::dmat4 transformation;  // placement
    };

    class IfcGeometryExtractor {
    public:
        explicit IfcGeometryExtractor(const IfcModel& model);
        std::optional<TriangulatedMesh> ExtractMesh(uint32_t elementId) const;
        std::vector<std::pair<uint32_t, TriangulatedMesh>> ExtractAll() const;
    };
}

// scene/include/scene/Scene.h
namespace bimeup::scene {
    using NodeId = uint32_t;

    struct SceneNode {
        NodeId id;
        std::string name;
        std::string ifcType;
        std::string globalId;
        glm::mat4 transform;
        AABB bounds;
        std::optional<MeshHandle> mesh;
        NodeId parent;
        std::vector<NodeId> children;
        bool visible = true;
        bool selected = false;
    };

    class Scene {
    public:
        NodeId AddNode(SceneNode node);
        SceneNode& GetNode(NodeId id);
        const SceneNode& GetNode(NodeId id) const;
        std::vector<NodeId> GetRoots() const;
        std::vector<NodeId> GetChildren(NodeId id) const;
        AABB GetBounds() const;
        size_t GetNodeCount() const;
        void SetVisibility(NodeId id, bool visible, bool recursive = false);
        void SetSelected(NodeId id, bool selected);
        std::vector<NodeId> GetSelected() const;
        std::vector<NodeId> FindByType(const std::string& ifcType) const;
    };
}
```

---

