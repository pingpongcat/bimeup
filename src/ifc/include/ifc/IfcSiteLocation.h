#pragma once

#include <optional>

namespace bimeup::ifc {

class IfcModel;

struct SiteLocation {
    double latitudeRad;
    double longitudeRad;
    double elevationM;
    // Angle from the model's local +Y axis to true north, measured CCW
    // around the local +Z axis (IFC up). 0 means local +Y == true north.
    double trueNorthRad;
};

// Reads `IfcSite.RefLatitude` / `RefLongitude` / `RefElevation` (DMS tuples
// + REAL) and the project context's `TrueNorth` (`IfcDirection` in the local
// XY plane). Returns `std::nullopt` if the file has no `IfcSite` or any of
// `RefLatitude` / `RefLongitude` / `RefElevation` is missing. `TrueNorth` is
// optional and defaults to 0 when absent.
std::optional<SiteLocation> ExtractSiteLocation(IfcModel& model);

// Pure helpers — exposed for unit testing the parsing math without a loader.

// `IfcCompoundPlaneAngleMeasure` is `[degrees, minutes, seconds, μ-seconds]`.
// All four components share the sign of `degrees` per IFC4 §8.10.3.4. Returns
// the angle in radians.
double DmsToRadians(int degrees, int minutes, int seconds, int microSeconds);

// `IfcGeometricRepresentationContext.TrueNorth` is a 2D `IfcDirection` in
// local XY giving the true-north direction expressed in the local frame.
// Returns the angle from local +Y to that direction, CCW around +Z.
//   (0, 1) →  0;   (1, 0) → −π/2;   (−1, 0) → +π/2;   (0, −1) → ±π.
double TrueNorthAngleFromDirection(double dirX, double dirY);

}  // namespace bimeup::ifc
