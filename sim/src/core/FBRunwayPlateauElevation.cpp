#include "FBRunwayPlateauElevation.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kPlateauMarginM = 5000.0;   /* full-elevation margin beyond the runway rectangle */
constexpr double kFalloffM = 10000.0;        /* smoothstep distance from full elevation to the base */

/* Distance (m) from (lat,lon) to `rwy`'s length x width rectangle (0 inside it) — along/across
 * projection onto the runway's centerline axis, same convention FBAppNative's OnRunway crash-gate
 * uses, but returning a distance-to-rectangle instead of an inside/outside bool. */
double FootprintDistM(const FBRunway &rwy, double latDeg, double lonDeg) {
  double hdg = rwy.TrueHeadingDeg * kPi / 180.0;
  double coslat = std::cos(rwy.ThresholdLatDeg * kPi / 180.0);
  double dy = (latDeg - rwy.ThresholdLatDeg) * 111320.0;
  double dx = (lonDeg - rwy.ThresholdLonDeg) * 111320.0 * coslat;
  double along = dx * std::sin(hdg) + dy * std::cos(hdg);
  double across = dx * std::cos(hdg) - dy * std::sin(hdg);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5;
  double lenM = rwy.LengthM > 1.0 ? rwy.LengthM : 0.0;
  double dAlong = along < 0.0 ? -along : (along > lenM ? along - lenM : 0.0);
  double absAcross = std::fabs(across);
  double dAcross = absAcross > halfW ? absAcross - halfW : 0.0;
  return std::sqrt(dAlong * dAlong + dAcross * dAcross);
}

/* 1 inside the plateau margin, 0 beyond plateau+falloff, smoothstep in between. */
double PlateauWeight(double distM) {
  if (distM <= kPlateauMarginM) return 1.0;
  if (distM >= kPlateauMarginM + kFalloffM) return 0.0;
  double t = (distM - kPlateauMarginM) / kFalloffM;
  return 1.0 - (3.0 * t * t - 2.0 * t * t * t);
}
} // namespace

double FBRunwayPlateauElevation::GroundElevM(double latDeg, double lonDeg) const {
  if (Runways.empty()) return BaseElevM;
  const FBRunway *nearest = &Runways[0];
  double nearestDist = FootprintDistM(Runways[0], latDeg, lonDeg);
  for (size_t i = 1; i < Runways.size(); i++) {
    double d = FootprintDistM(Runways[i], latDeg, lonDeg);
    if (d < nearestDist) { nearestDist = d; nearest = &Runways[i]; }
  }
  double w = PlateauWeight(nearestDist);
  return BaseElevM + w * (nearest->ThresholdElevM - BaseElevM);
}

} // namespace FlightBox
