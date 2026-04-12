#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bimeup::ifc
