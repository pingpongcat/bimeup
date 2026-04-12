#include <gtest/gtest.h>
#include <web-ifc/modelmanager/ModelManager.h>
#include <web-ifc/parsing/IfcLoader.h>
#include <web-ifc/schema/IfcSchemaManager.h>
#include <fstream>

TEST(WebIfcIntegration, ModelManagerCreates) {
    webifc::manager::ModelManager manager(false);
    webifc::manager::LoaderSettings settings;
    uint32_t modelID = manager.CreateModel(settings);
    EXPECT_TRUE(manager.IsModelOpen(modelID));
    manager.CloseModel(modelID);
    EXPECT_FALSE(manager.IsModelOpen(modelID));
}

TEST(WebIfcIntegration, SchemaManagerHasTypes) {
    webifc::schema::IfcSchemaManager schemaManager;
    // IFCWALL is a well-known IFC type — verify the schema knows about it
    auto typeName = schemaManager.IfcTypeCodeToType(webifc::schema::IFCWALL);
    EXPECT_FALSE(typeName.empty());
}

TEST(WebIfcIntegration, LoadIfcFile) {
    webifc::manager::ModelManager manager(false);
    webifc::manager::LoaderSettings settings;
    uint32_t modelID = manager.CreateModel(settings);

    // Load the example IFC file
    std::string ifcPath = std::string(TEST_DATA_DIR) + "/example.ifc";
    std::ifstream file(ifcPath, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Could not open test IFC file: " << ifcPath;

    auto* loader = manager.GetIfcLoader(modelID);
    ASSERT_NE(loader, nullptr);
    loader->LoadFile(file);

    // Should have parsed some IFC entities
    EXPECT_GT(loader->GetMaxExpressId(), 0u);

    manager.CloseModel(modelID);
}
