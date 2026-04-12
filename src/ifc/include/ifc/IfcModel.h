#pragma once

#include "ifc/IfcElement.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace webifc::geometry { class IfcGeometryProcessor; }
namespace webifc::parsing { class IfcLoader; }
namespace webifc::schema { class IfcSchemaManager; }

namespace bimeup::ifc {

class IfcModel {
public:
    IfcModel();
    ~IfcModel();

    IfcModel(const IfcModel&) = delete;
    IfcModel& operator=(const IfcModel&) = delete;
    IfcModel(IfcModel&&) noexcept;
    IfcModel& operator=(IfcModel&&) noexcept;

    bool LoadFromFile(const std::string& path);
    bool IsLoaded() const;

    size_t GetElementCount() const;
    std::vector<uint32_t> GetElementExpressIds() const;
    std::vector<uint32_t> GetExpressIdsByType(const std::string& ifcType) const;

    std::string GetElementType(uint32_t expressId) const;
    std::string GetGlobalId(uint32_t expressId) const;
    std::string GetElementName(uint32_t expressId) const;

    std::optional<IfcElement> GetElement(uint32_t expressId) const;
    std::vector<IfcElement> GetElements() const;
    std::vector<IfcElement> GetElementsByType(const std::string& ifcType) const;
    std::optional<IfcElement> GetElementByGlobalId(const std::string& guid) const;

    // Internal: access geometry processor for mesh extraction
    webifc::geometry::IfcGeometryProcessor* GetGeometryProcessor();

    // Internal: access loader for relationship/property queries
    webifc::parsing::IfcLoader* GetLoader();

    // Internal: access schema manager for type code lookups
    const webifc::schema::IfcSchemaManager& GetSchemaManager() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bimeup::ifc
