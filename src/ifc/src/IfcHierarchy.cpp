#include "ifc/IfcHierarchy.h"
#include "ifc/IfcModel.h"

#include <web-ifc/parsing/IfcLoader.h>
#include <web-ifc/schema/IfcSchemaManager.h>
#include <web-ifc/schema/ifc-schema.h>

#include <algorithm>
#include <unordered_map>

namespace bimeup::ifc {

namespace {

// Resolve a set of tape offsets to express IDs via GetRefArgument(offset)
std::vector<uint32_t>
ResolveRefSet(webifc::parsing::IfcLoader* loader,
              const std::vector<uint32_t>& tapeOffsets) {
    std::vector<uint32_t> ids;
    ids.reserve(tapeOffsets.size());
    for (uint32_t offset : tapeOffsets) {
        ids.push_back(loader->GetRefArgument(offset));
    }
    return ids;
}

// Build a map: parent expressId -> list of child expressIds
// from IfcRelAggregates relationships
std::unordered_map<uint32_t, std::vector<uint32_t>>
BuildAggregatesMap(webifc::parsing::IfcLoader* loader) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> map;

    const auto& relIds = loader->GetExpressIDsWithType(
        webifc::schema::IFCRELAGGREGATES);

    for (uint32_t relId : relIds) {
        try {
            // Arg 4: RelatingObject (parent ref)
            loader->MoveToLineArgument(relId, 4);
            uint32_t parentId = loader->GetRefArgument();

            // Arg 5: RelatedObjects (set of child refs)
            // GetSetArgument returns tape offsets, not express IDs
            loader->MoveToLineArgument(relId, 5);
            auto childOffsets = loader->GetSetArgument();
            auto childIds = ResolveRefSet(loader, childOffsets);

            auto& children = map[parentId];
            children.insert(children.end(), childIds.begin(), childIds.end());
        } catch (...) {
            continue;
        }
    }

    return map;
}

// Build a map: spatial structure expressId -> list of contained element expressIds
// from IfcRelContainedInSpatialStructure relationships
std::unordered_map<uint32_t, std::vector<uint32_t>>
BuildContainmentMap(webifc::parsing::IfcLoader* loader) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> map;

    const auto& relIds = loader->GetExpressIDsWithType(
        webifc::schema::IFCRELCONTAINEDINSPATIALSTRUCTURE);

    for (uint32_t relId : relIds) {
        try {
            // Arg 4: RelatedElements (set of element refs)
            // GetSetArgument returns tape offsets, not express IDs
            loader->MoveToLineArgument(relId, 4);
            auto elementOffsets = loader->GetSetArgument();
            auto elementIds = ResolveRefSet(loader, elementOffsets);

            // Arg 5: RelatingStructure (spatial structure ref)
            loader->MoveToLineArgument(relId, 5);
            uint32_t structureId = loader->GetRefArgument();

            auto& elements = map[structureId];
            elements.insert(elements.end(), elementIds.begin(), elementIds.end());
        } catch (...) {
            continue;
        }
    }

    return map;
}

HierarchyNode MakeNode(uint32_t expressId,
                        webifc::parsing::IfcLoader* loader,
                        const webifc::schema::IfcSchemaManager& schema) {
    HierarchyNode node;
    node.expressId = expressId;

    uint32_t typeCode = loader->GetLineType(expressId);
    node.type = schema.IfcTypeCodeToType(typeCode);

    // GlobalId is argument 0 for IfcRoot-derived entities
    try {
        loader->MoveToLineArgument(expressId, 0);
        node.globalId = std::string(loader->GetStringArgument());
    } catch (...) {}

    // Name is argument 2
    try {
        loader->MoveToLineArgument(expressId, 2);
        if (loader->GetTokenType() != webifc::parsing::IfcTokenType::EMPTY) {
            node.name = std::string(loader->GetStringArgument());
        }
    } catch (...) {}

    return node;
}

void BuildSubtree(HierarchyNode& node,
                  webifc::parsing::IfcLoader* loader,
                  const webifc::schema::IfcSchemaManager& schema,
                  const std::unordered_map<uint32_t, std::vector<uint32_t>>& aggregates,
                  const std::unordered_map<uint32_t, std::vector<uint32_t>>& containment) {
    // Add aggregated children (Site, Building, Storey)
    auto aggIt = aggregates.find(node.expressId);
    if (aggIt != aggregates.end()) {
        for (uint32_t childId : aggIt->second) {
            auto child = MakeNode(childId, loader, schema);
            BuildSubtree(child, loader, schema, aggregates, containment);
            node.children.push_back(std::move(child));
        }
    }

    // Add contained elements (walls, columns, etc.)
    auto contIt = containment.find(node.expressId);
    if (contIt != containment.end()) {
        for (uint32_t elemId : contIt->second) {
            node.children.push_back(MakeNode(elemId, loader, schema));
        }
    }
}

} // anonymous namespace

IfcHierarchy::IfcHierarchy(IfcModel& model) {
    Build(model);
}

void IfcHierarchy::Build(IfcModel& model) {
    auto* loader = model.GetLoader();
    if (!loader) {
        return;
    }

    const auto& schema = model.GetSchemaManager();

    auto aggregates = BuildAggregatesMap(loader);
    auto containment = BuildContainmentMap(loader);

    // Find the IfcProject root
    const auto& projectIds = loader->GetExpressIDsWithType(
        webifc::schema::IFCPROJECT);

    if (projectIds.empty()) {
        return;
    }

    root_ = MakeNode(projectIds[0], loader, schema);
    BuildSubtree(root_, loader, schema, aggregates, containment);
}

const HierarchyNode& IfcHierarchy::GetRoot() const {
    return root_;
}

size_t IfcHierarchy::GetDepth() const {
    return ComputeDepth(root_);
}

size_t IfcHierarchy::GetElementCount() const {
    return CountElements(root_, false);
}

const HierarchyNode* IfcHierarchy::FindNode(uint32_t expressId) const {
    return FindInSubtree(root_, expressId);
}

size_t IfcHierarchy::ComputeDepth(const HierarchyNode& node) {
    if (node.children.empty()) {
        return 1;
    }

    size_t maxChildDepth = 0;
    for (const auto& child : node.children) {
        maxChildDepth = std::max(maxChildDepth, ComputeDepth(child));
    }
    return 1 + maxChildDepth;
}

size_t IfcHierarchy::CountElements(const HierarchyNode& node, bool isLeafLevel) {
    // Leaf nodes (no children) that are not spatial structure types are elements
    if (node.children.empty()) {
        // Only count if this isn't the root or a spatial container
        return isLeafLevel ? 1 : 0;
    }

    size_t count = 0;
    for (const auto& child : node.children) {
        count += CountElements(child, child.children.empty());
    }
    return count;
}

const HierarchyNode* IfcHierarchy::FindInSubtree(const HierarchyNode& node,
                                                   uint32_t expressId) {
    if (node.expressId == expressId) {
        return &node;
    }

    for (const auto& child : node.children) {
        if (const auto* found = FindInSubtree(child, expressId)) {
            return found;
        }
    }

    return nullptr;
}

} // namespace bimeup::ifc
