#include <gtest/gtest.h>
#include <ifc/IfcGeometryExtractor.h>
#include <ifc/IfcModel.h>

class IfcGeometryExtractorTest : public ::testing::Test {
protected:
    static constexpr const char* kTestFile = TEST_DATA_DIR "/example.ifc";

    bimeup::ifc::IfcModel model;

    void SetUp() override {
        ASSERT_TRUE(model.LoadFromFile(kTestFile));
    }
};

TEST_F(IfcGeometryExtractorTest, ExtractMesh_ValidElement_ReturnsMesh) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    // Try to extract geometry from each element until we find one with geometry
    std::optional<bimeup::ifc::TriangulatedMesh> mesh;
    for (uint32_t id : ids) {
        mesh = extractor.ExtractMesh(id);
        if (mesh.has_value()) {
            break;
        }
    }

    ASSERT_TRUE(mesh.has_value());
    EXPECT_GT(mesh->positions.size(), 0u);
    EXPECT_GT(mesh->normals.size(), 0u);
    EXPECT_GT(mesh->indices.size(), 0u);
    EXPECT_EQ(mesh->positions.size(), mesh->normals.size());
}

TEST_F(IfcGeometryExtractorTest, ExtractMesh_InvalidId_ReturnsNullopt) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto mesh = extractor.ExtractMesh(999999);
    EXPECT_FALSE(mesh.has_value());
}

TEST_F(IfcGeometryExtractorTest, ExtractMesh_HasValidIndices) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    std::optional<bimeup::ifc::TriangulatedMesh> mesh;
    for (uint32_t id : ids) {
        mesh = extractor.ExtractMesh(id);
        if (mesh.has_value()) {
            break;
        }
    }

    ASSERT_TRUE(mesh.has_value());
    // All indices must be valid (less than position count)
    for (uint32_t idx : mesh->indices) {
        EXPECT_LT(idx, mesh->positions.size());
    }
    // Triangle mesh: index count must be multiple of 3
    EXPECT_EQ(mesh->indices.size() % 3, 0u);
}

TEST_F(IfcGeometryExtractorTest, ExtractMesh_HasTransformation) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    std::optional<bimeup::ifc::TriangulatedMesh> mesh;
    for (uint32_t id : ids) {
        mesh = extractor.ExtractMesh(id);
        if (mesh.has_value()) {
            break;
        }
    }

    ASSERT_TRUE(mesh.has_value());
    // Transformation should be set (not all zeros)
    EXPECT_NE(mesh->transformation, glm::dmat4(0.0));
}

TEST_F(IfcGeometryExtractorTest, ExtractAll_ReturnsMultipleMeshes) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto meshes = extractor.ExtractAll();
    EXPECT_GT(meshes.size(), 0u);

    // Each returned mesh should have valid geometry
    for (const auto& [expressId, mesh] : meshes) {
        EXPECT_GT(mesh.positions.size(), 0u);
        EXPECT_GT(mesh.indices.size(), 0u);
        EXPECT_EQ(mesh.positions.size(), mesh.normals.size());
        EXPECT_GT(expressId, 0u);
    }
}

TEST_F(IfcGeometryExtractorTest, ExtractMesh_IfcColumn_HasGeometry) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());

    auto mesh = extractor.ExtractMesh(columns[0]);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_GT(mesh->positions.size(), 0u);
    EXPECT_GT(mesh->indices.size(), 0u);
}

// The example IFC assigns IFCSURFACESTYLE "Concrete, Cast-in-Place gray"
// (IFCCOLOURRGB 0.7529, 0.7529, 0.7529) to IfcColumn elements via
// IFCSTYLEDITEM → IFCPRESENTATIONSTYLEASSIGNMENT. The extractor must surface
// that color on the TriangulatedMesh.
TEST_F(IfcGeometryExtractorTest, ExtractMesh_IfcColumn_PicksUpSurfaceStyleColor) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto columns = model.GetExpressIdsByType("IFCCOLUMN");
    ASSERT_FALSE(columns.empty());

    std::optional<bimeup::ifc::TriangulatedMesh> mesh;
    for (uint32_t id : columns) {
        mesh = extractor.ExtractMesh(id);
        if (mesh.has_value()) {
            break;
        }
    }
    ASSERT_TRUE(mesh.has_value());

    constexpr double kExpected = 0.752941176470588;
    constexpr double kTol = 1e-3;
    EXPECT_NEAR(mesh->color.r, kExpected, kTol);
    EXPECT_NEAR(mesh->color.g, kExpected, kTol);
    EXPECT_NEAR(mesh->color.b, kExpected, kTol);
    EXPECT_GT(mesh->color.a, 0.0);
}

TEST_F(IfcGeometryExtractorTest, ExtractSubMeshes_InvalidId_ReturnsEmpty) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto subs = extractor.ExtractSubMeshes(999999);
    EXPECT_TRUE(subs.empty());
}

TEST_F(IfcGeometryExtractorTest, ExtractSubMeshes_ValidElement_ReturnsAtLeastOne) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    std::vector<bimeup::ifc::TriangulatedMesh> subs;
    for (uint32_t id : ids) {
        subs = extractor.ExtractSubMeshes(id);
        if (!subs.empty()) {
            break;
        }
    }

    ASSERT_FALSE(subs.empty());
    for (const auto& s : subs) {
        EXPECT_GT(s.positions.size(), 0u);
        EXPECT_EQ(s.positions.size(), s.normals.size());
        EXPECT_EQ(s.indices.size() % 3, 0u);
        for (uint32_t idx : s.indices) {
            EXPECT_LT(idx, s.positions.size());
        }
    }
}

// The union of sub-mesh vertices (per element) must match the merged-mesh
// vertex count that the legacy ExtractMesh returns. Guarantees we don't drop
// or duplicate geometry when switching to the per-placed-geom split.
TEST_F(IfcGeometryExtractorTest, ExtractSubMeshes_TotalVerticesMatchMergedMesh) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    size_t elementsChecked = 0;
    for (uint32_t id : ids) {
        auto merged = extractor.ExtractMesh(id);
        auto subs = extractor.ExtractSubMeshes(id);

        if (!merged.has_value()) {
            EXPECT_TRUE(subs.empty()) << "expressId " << id;
            continue;
        }
        ASSERT_FALSE(subs.empty()) << "expressId " << id;

        size_t totalVerts = 0;
        size_t totalIndices = 0;
        for (const auto& s : subs) {
            totalVerts += s.positions.size();
            totalIndices += s.indices.size();
        }
        EXPECT_EQ(totalVerts, merged->positions.size()) << "expressId " << id;
        EXPECT_EQ(totalIndices, merged->indices.size()) << "expressId " << id;
        ++elementsChecked;
    }
    EXPECT_GT(elementsChecked, 0u);
}

TEST_F(IfcGeometryExtractorTest, ExtractSubMeshes_EachHasValidColor) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto ids = model.GetElementExpressIds();
    ASSERT_FALSE(ids.empty());

    bool anyChecked = false;
    for (uint32_t id : ids) {
        auto subs = extractor.ExtractSubMeshes(id);
        for (const auto& s : subs) {
            EXPECT_GE(s.color.r, 0.0);
            EXPECT_LE(s.color.r, 1.0);
            EXPECT_GE(s.color.g, 0.0);
            EXPECT_LE(s.color.g, 1.0);
            EXPECT_GE(s.color.b, 0.0);
            EXPECT_LE(s.color.b, 1.0);
            EXPECT_GT(s.color.a, 0.0);
            EXPECT_LE(s.color.a, 1.0);
            anyChecked = true;
        }
    }
    EXPECT_TRUE(anyChecked);
}

TEST_F(IfcGeometryExtractorTest, ExtractAll_ColorsAreInUnitRange) {
    bimeup::ifc::IfcGeometryExtractor extractor(model);

    auto meshes = extractor.ExtractAll();
    ASSERT_FALSE(meshes.empty());

    for (const auto& [expressId, mesh] : meshes) {
        EXPECT_GE(mesh.color.r, 0.0);
        EXPECT_LE(mesh.color.r, 1.0);
        EXPECT_GE(mesh.color.g, 0.0);
        EXPECT_LE(mesh.color.g, 1.0);
        EXPECT_GE(mesh.color.b, 0.0);
        EXPECT_LE(mesh.color.b, 1.0);
        EXPECT_GT(mesh.color.a, 0.0);
        EXPECT_LE(mesh.color.a, 1.0);
    }
}
