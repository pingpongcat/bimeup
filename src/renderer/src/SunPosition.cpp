#include <renderer/SunPosition.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bimeup::renderer {

namespace {

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

// Wrap to [0, 360).
double Mod360(double x) {
    double r = std::fmod(x, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    return r;
}

}  // namespace

SunPosition ComputeSunDirection(double julianDay,
                                double latitudeRad,
                                double longitudeRad,
                                double trueNorthRad) {
    // NOAA Solar Calculator algorithm. Reference:
    //   https://gml.noaa.gov/grad/solcalc/calcdetails.html
    // Formulae below mirror that spreadsheet column-for-column.

    const double t = (julianDay - 2451545.0) / 36525.0;  // Julian centuries

    // Geometric mean longitude (deg).
    const double l0 =
        Mod360(280.46646 + (t * (36000.76983 + (0.0003032 * t))));

    // Mean anomaly (deg).
    const double m = 357.52911 + (t * (35999.05029 - (0.0001537 * t)));

    // Earth-orbit eccentricity (unitless).
    const double e =
        0.016708634 - (t * (0.000042037 + (0.0000001267 * t)));

    // Equation of centre (deg). M in radians for the trig terms.
    const double mr = m * kDegToRad;
    const double c = (std::sin(mr) *
                      (1.914602 - (t * (0.004817 + (0.000014 * t))))) +
                     (std::sin(2.0 * mr) *
                      (0.019993 - (0.000101 * t))) +
                     (std::sin(3.0 * mr) * 0.000289);

    // True longitude (deg) and apparent longitude (deg).
    const double trueLong = l0 + c;
    const double omegaDeg = 125.04 - (1934.136 * t);
    const double omegaRad = omegaDeg * kDegToRad;
    const double appLong =
        trueLong - 0.00569 - (0.00478 * std::sin(omegaRad));

    // Mean obliquity of the ecliptic (deg). Seconds-of-arc form per NOAA.
    const double seconds =
        21.448 - (t * (46.815 + (t * (0.00059 - (t * 0.001813)))));
    const double eps0 = 23.0 + ((26.0 + (seconds / 60.0)) / 60.0);
    const double eps = eps0 + (0.00256 * std::cos(omegaRad));

    const double epsRad = eps * kDegToRad;
    const double appLongRad = appLong * kDegToRad;

    // Declination (rad).
    const double declination =
        std::asin(std::sin(epsRad) * std::sin(appLongRad));

    // Equation of time (minutes). The bracketed expression is in radians;
    // multiplying by 4 · (180/π) converts to minutes (1° = 4 min of time).
    const double y = std::tan(epsRad * 0.5) * std::tan(epsRad * 0.5);
    const double l0Rad = l0 * kDegToRad;
    const double eqTimeRad = (y * std::sin(2.0 * l0Rad)) -
                             (2.0 * e * std::sin(mr)) +
                             (4.0 * e * y * std::sin(mr) *
                              std::cos(2.0 * l0Rad)) -
                             (0.5 * y * y * std::sin(4.0 * l0Rad)) -
                             (1.25 * e * e * std::sin(2.0 * mr));
    const double eqTimeMin = 4.0 * kRadToDeg * eqTimeRad;

    // True solar time (minutes) at the location.
    const double utcDayFraction =
        (julianDay + 0.5) - std::floor(julianDay + 0.5);
    const double utcMinutes = utcDayFraction * 1440.0;
    const double tstMinutes =
        utcMinutes + eqTimeMin + (4.0 * (longitudeRad * kRadToDeg));

    // Hour angle (rad), wrapped to (−π, π].
    double hourAngleDeg = (tstMinutes / 4.0) - 180.0;
    hourAngleDeg = std::fmod(hourAngleDeg, 360.0);
    if (hourAngleDeg > 180.0) {
        hourAngleDeg -= 360.0;
    } else if (hourAngleDeg < -180.0) {
        hourAngleDeg += 360.0;
    }
    const double hourAngle = hourAngleDeg * kDegToRad;

    // Elevation (rad).
    const double sinLat = std::sin(latitudeRad);
    const double cosLat = std::cos(latitudeRad);
    const double sinDec = std::sin(declination);
    const double cosDec = std::cos(declination);
    const double cosZenith = std::clamp(
        (sinLat * sinDec) + (cosLat * cosDec * std::cos(hourAngle)), -1.0,
        1.0);
    const double zenith = std::acos(cosZenith);
    const double elevation = (kPi * 0.5) - zenith;

    // Geographic azimuth (rad), CW from true north, in [0, 2π).
    const double cosEl = std::cos(elevation);
    double geoAzimuth;
    if (cosEl < 1e-9 || cosLat < 1e-9) {
        // Sun at the zenith or observer at a pole — azimuth is undefined.
        // Pin to 0 deterministically; the elevation result is still valid.
        geoAzimuth = 0.0;
    } else {
        const double cosAz = std::clamp(
            (sinDec - (std::sin(elevation) * sinLat)) / (cosEl * cosLat),
            -1.0, 1.0);
        geoAzimuth = std::acos(cosAz);
        if (hourAngle > 0.0) {
            geoAzimuth = kTwoPi - geoAzimuth;
        }
    }

    // Apply TrueNorth: rotate azimuth and dirWorld together so that the
    // observed sun stays anchored to the sky as the model frame rotates.
    double modelAzimuth = std::fmod(geoAzimuth + trueNorthRad, kTwoPi);
    if (modelAzimuth < 0.0) {
        modelAzimuth += kTwoPi;
    }

    // Direction TO the sun in the model frame (right-handed, +Y up):
    //   horizontal: (sin az, 0, cos az) — matches CW-from-+Z convention.
    //   vertical:   y = sin(elevation).
    const float fAz = static_cast<float>(modelAzimuth);
    const float fEl = static_cast<float>(elevation);
    const float cAz = std::cos(fAz);
    const float sAz = std::sin(fAz);
    const float cElF = std::cos(fEl);
    const float sElF = std::sin(fEl);

    SunPosition result;
    // FROM sun TO scene = -(TO sun).
    result.dirWorld = glm::vec3(-sAz * cElF, -sElF, -cAz * cElF);
    result.elevation = fEl;
    result.azimuth = fAz;
    return result;
}

}  // namespace bimeup::renderer
