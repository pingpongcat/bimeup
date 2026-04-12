#include "ifc/IfcModel.h"

#include <web-ifc/modelmanager/ModelManager.h>
#include <web-ifc/parsing/IfcLoader.h>
#include <web-ifc/schema/IfcSchemaManager.h>
#include <web-ifc/schema/ifc-schema.h>

#include <fstream>

namespace bimeup::ifc {

struct IfcModel::Impl {
    webifc::manager::ModelManager manager{false};
    uint32_t modelId = 0;
    bool loaded = false;

    // Cached element express IDs (products only)
    std::vector<uint32_t> elementIds;

    webifc::parsing::IfcLoader* GetLoader() const {
        return manager.GetIfcLoader(modelId);
    }
};

IfcModel::IfcModel() : impl_(std::make_unique<Impl>()) {}

IfcModel::~IfcModel() {
    if (impl_ && impl_->loaded) {
        impl_->manager.CloseModel(impl_->modelId);
    }
}

IfcModel::IfcModel(IfcModel&&) noexcept = default;
IfcModel& IfcModel::operator=(IfcModel&&) noexcept = default;

bool IfcModel::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }

    webifc::manager::LoaderSettings settings;
    impl_->modelId = impl_->manager.CreateModel(settings);

    auto* loader = impl_->GetLoader();
    if (!loader) {
        return false;
    }

    try {
        loader->LoadFile(file);
    } catch (...) {
        impl_->manager.CloseModel(impl_->modelId);
        return false;
    }

    // Collect all element express IDs using the schema manager's element list
    const auto& schemaManager = impl_->manager.GetSchemaManager();
    const auto& ifcElements = schemaManager.GetIfcElementList();

    for (uint32_t typeCode : ifcElements) {
        const auto& ids = loader->GetExpressIDsWithType(typeCode);
        impl_->elementIds.insert(impl_->elementIds.end(), ids.begin(), ids.end());
    }

    impl_->loaded = true;
    return true;
}

bool IfcModel::IsLoaded() const {
    return impl_->loaded;
}

size_t IfcModel::GetElementCount() const {
    return impl_->elementIds.size();
}

std::vector<uint32_t> IfcModel::GetElementExpressIds() const {
    return impl_->elementIds;
}

std::vector<uint32_t> IfcModel::GetExpressIdsByType(const std::string& ifcType) const {
    if (!impl_->loaded) {
        return {};
    }

    const auto& schemaManager = impl_->manager.GetSchemaManager();
    uint32_t typeCode = schemaManager.IfcTypeToTypeCode(ifcType);
    if (typeCode == 0) {
        return {};
    }

    auto* loader = impl_->GetLoader();
    const auto& ids = loader->GetExpressIDsWithType(typeCode);
    return {ids.begin(), ids.end()};
}

std::string IfcModel::GetElementType(uint32_t expressId) const {
    if (!impl_->loaded) {
        return {};
    }

    auto* loader = impl_->GetLoader();
    if (!loader->IsValidExpressID(expressId)) {
        return {};
    }

    uint32_t typeCode = loader->GetLineType(expressId);
    const auto& schemaManager = impl_->manager.GetSchemaManager();
    return schemaManager.IfcTypeCodeToType(typeCode);
}

std::string IfcModel::GetGlobalId(uint32_t expressId) const {
    if (!impl_->loaded) {
        return {};
    }

    auto* loader = impl_->GetLoader();
    if (!loader->IsValidExpressID(expressId)) {
        return {};
    }

    try {
        // GlobalId is argument 0 for IfcRoot-derived entities
        loader->MoveToLineArgument(expressId, 0);
        return std::string(loader->GetStringArgument());
    } catch (...) {
        return {};
    }
}

std::string IfcModel::GetElementName(uint32_t expressId) const {
    if (!impl_->loaded) {
        return {};
    }

    auto* loader = impl_->GetLoader();
    if (!loader->IsValidExpressID(expressId)) {
        return {};
    }

    try {
        // Name is argument 2 for IfcRoot-derived entities
        // (0=GlobalId, 1=OwnerHistory, 2=Name, 3=Description)
        loader->MoveToLineArgument(expressId, 2);
        if (loader->GetTokenType() == webifc::parsing::IfcTokenType::EMPTY) {
            return {};
        }
        return std::string(loader->GetStringArgument());
    } catch (...) {
        return {};
    }
}

} // namespace bimeup::ifc
