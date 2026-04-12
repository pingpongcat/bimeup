#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bimeup::ifc {

class IfcModel;

struct HierarchyNode {
    uint32_t expressId = 0;
    std::string type;
    std::string name;
    std::string globalId;
    std::vector<HierarchyNode> children;
};

class IfcHierarchy {
public:
    explicit IfcHierarchy(IfcModel& model);

    const HierarchyNode& GetRoot() const;
    size_t GetDepth() const;
    size_t GetElementCount() const;
    const HierarchyNode* FindNode(uint32_t expressId) const;

private:
    HierarchyNode root_;

    void Build(IfcModel& model);

    static size_t ComputeDepth(const HierarchyNode& node);
    static size_t CountElements(const HierarchyNode& node,
                                bool isLeafLevel);
    static const HierarchyNode* FindInSubtree(const HierarchyNode& node,
                                               uint32_t expressId);
};

} // namespace bimeup::ifc
