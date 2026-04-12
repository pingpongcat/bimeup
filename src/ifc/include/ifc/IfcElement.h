#pragma once

#include <cstdint>
#include <string>

namespace bimeup::ifc {

struct IfcElement {
    uint32_t expressId = 0;
    std::string globalId;
    std::string type;
    std::string name;
};

} // namespace bimeup::ifc
