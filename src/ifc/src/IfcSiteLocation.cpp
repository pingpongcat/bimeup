#include "ifc/IfcSiteLocation.h"
#include "ifc/IfcModel.h"

#include <web-ifc/parsing/IfcLoader.h>
#include <web-ifc/schema/IfcSchemaManager.h>
#include <web-ifc/schema/ifc-schema.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>

namespace bimeup::ifc {

namespace {

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kHalfPi = kPi * 0.5;

// Reads an `IfcCompoundPlaneAngleMeasure` from argument `argIndex` of the
// given line. Returns nullopt when the slot is `$` or the list is too short.
std::optional<double> ReadCompoundPlaneAngle(
    webifc::parsing::IfcLoader* loader, std::uint32_t expressId,
    std::uint32_t argIndex) {
    try {
        loader->MoveToLineArgument(expressId, argIndex);
        if (loader->GetTokenType() ==
            webifc::parsing::IfcTokenType::EMPTY) {
            return std::nullopt;
        }
        loader->StepBack();  // GetTokenType consumed the type byte
        const auto offsets = loader->GetSetArgument();
        if (offsets.size() < 3) {
            return std::nullopt;
        }
        const int degrees =
            static_cast<int>(loader->GetIntArgument(offsets[0]));
        const int minutes =
            static_cast<int>(loader->GetIntArgument(offsets[1]));
        const int seconds =
            static_cast<int>(loader->GetIntArgument(offsets[2]));
        const int micros =
            offsets.size() >= 4
                ? static_cast<int>(loader->GetIntArgument(offsets[3]))
                : 0;
        return DmsToRadians(degrees, minutes, seconds, micros);
    } catch (...) {
        return std::nullopt;
    }
}

// Walks `IfcGeometricRepresentationContext` lines (NOT subcontexts — those
// inherit but use a different type code) and returns the `TrueNorth` angle
// from the first context that has one. 0 if no context defines TrueNorth.
double ExtractTrueNorth(webifc::parsing::IfcLoader* loader) {
    const auto& contextIds = loader->GetExpressIDsWithType(
        webifc::schema::IFCGEOMETRICREPRESENTATIONCONTEXT);
    for (std::uint32_t cid : contextIds) {
        try {
            loader->MoveToLineArgument(cid, 5);  // TrueNorth
            if (loader->GetTokenType() ==
                webifc::parsing::IfcTokenType::EMPTY) {
                continue;
            }
            loader->StepBack();
            const std::uint32_t dirRef = loader->GetRefArgument();

            loader->MoveToLineArgument(dirRef, 0);  // DirectionRatios
            if (loader->GetTokenType() ==
                webifc::parsing::IfcTokenType::EMPTY) {
                continue;
            }
            loader->StepBack();
            const auto offsets = loader->GetSetArgument();
            if (offsets.size() < 2) {
                continue;
            }
            const double dx = loader->GetDoubleArgument(offsets[0]);
            const double dy = loader->GetDoubleArgument(offsets[1]);
            return TrueNorthAngleFromDirection(dx, dy);
        } catch (...) {
            continue;
        }
    }
    return 0.0;
}

}  // namespace

double DmsToRadians(int degrees, int minutes, int seconds, int microSeconds) {
    // Per IFC4 §8.10.3.4 the components share their sign with `degrees`,
    // so the magnitudes simply add.
    const double mag = static_cast<double>(std::abs(degrees)) +
                       (static_cast<double>(std::abs(minutes)) / 60.0) +
                       (static_cast<double>(std::abs(seconds)) / 3600.0) +
                       (static_cast<double>(std::abs(microSeconds)) /
                        (3600.0 * 1.0e6));
    const bool negative = (degrees < 0) || (minutes < 0) || (seconds < 0) ||
                          (microSeconds < 0);
    const double signedDeg = negative ? -mag : mag;
    return signedDeg * (kPi / 180.0);
}

double TrueNorthAngleFromDirection(double dirX, double dirY) {
    return std::atan2(dirY, dirX) - kHalfPi;
}

std::optional<SiteLocation> ExtractSiteLocation(IfcModel& model) {
    auto* loader = model.GetLoader();
    if (loader == nullptr) {
        return std::nullopt;
    }

    const auto& siteIds =
        loader->GetExpressIDsWithType(webifc::schema::IFCSITE);
    if (siteIds.empty()) {
        return std::nullopt;
    }
    const std::uint32_t siteId = siteIds[0];

    SiteLocation loc{};

    auto lat = ReadCompoundPlaneAngle(loader, siteId, 9);
    if (!lat) {
        return std::nullopt;
    }
    loc.latitudeRad = *lat;

    auto lon = ReadCompoundPlaneAngle(loader, siteId, 10);
    if (!lon) {
        return std::nullopt;
    }
    loc.longitudeRad = *lon;

    try {
        loader->MoveToLineArgument(siteId, 11);  // RefElevation (REAL)
        if (loader->GetTokenType() ==
            webifc::parsing::IfcTokenType::EMPTY) {
            return std::nullopt;
        }
        loader->StepBack();
        loc.elevationM = loader->GetDoubleArgument();
    } catch (...) {
        return std::nullopt;
    }

    loc.trueNorthRad = ExtractTrueNorth(loader);
    return loc;
}

}  // namespace bimeup::ifc
