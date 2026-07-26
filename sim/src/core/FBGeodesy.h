/* FlightBox — FBGeodesy: the ONE planar-ENU geodesy the simulator measures with. Header-only, no
 * translation unit.
 *
 * Why this file exists: the identical five-line "dlat*111320, dlon*111320*cos(lat)" block stood in six
 * places (core/FBMissionMonitor, core/FBRunwayPlateauElevation, systems/FBPilot, systems/FBNavSystem,
 * systems/FBAutopilot, app/FBAppWasm) — and only some of them wrapped the longitude difference into
 * [-180,180]. The unwrapped copies read a ~360 deg delta across the antimeridian, i.e. a distance of
 * ~38,000 km to a point a metre away: at 180 deg longitude the mission monitor's waypoint capture, the
 * runway-plateau elevation and the HUD's home distance were all simply wrong. Wrapping is now part of
 * the primitive, not something each caller has to remember.
 *
 * CONVENTION: the reference point comes FIRST and owns the cosine. FBEnuOffsetM(ref, p) returns p's
 * offset FROM ref, with the longitude scaling taken at the REFERENCE latitude — one rule, so a bearing
 * and a distance computed by two different subsystems agree. A caller that only needs a distance may
 * pass either point as the reference (the offsets differ only in sign, the magnitude is identical).
 *
 * SCOPE: deliberately planar/small-angle, matching what every call site already did — steerpoints,
 * runway axes and waypoint captures are tens of nautical miles out, not intercontinental. Real geodesic
 * math belongs to whatever needs it, not to this file's callers. */
#ifndef FBGEODESY_H
#define FBGEODESY_H

#include <cmath>
#include "FBUnits.h"

namespace FlightBox {

/* WGS84 geodetic -> ECEF (m). The planar helpers below are small-angle by design; this one is not — it
 * is the exact ellipsoid conversion the renderer's camera-relative ECEF world is built on. It stood
 * character-identical in both App entry points (FBAppNative.cpp, FBAppWasm.cpp) before it moved here. */
inline void FBGeoToEcef(double latDeg, double lonDeg, double altM, double out[3]) {
  const double a = 6378137.0, e2 = 6.69437999014e-3;
  double lat = latDeg * kDeg2Rad, lon = lonDeg * kDeg2Rad;
  double sl = std::sin(lat), cl = std::cos(lat);
  double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + altM) * cl * std::cos(lon);
  out[1] = (N + altM) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + altM) * sl;
}

/* The local ENU axes at (lat,lon), expressed in ECEF — the rotation every ECEF-space camera/vector
 * conversion starts from (render/FBCamera.h's FBCameraBasisEcef, the App screenshot paths). */
inline void FBEnuAxesEcef(double latDeg, double lonDeg, double E[3], double N[3], double U[3]) {
  double P = latDeg * kDeg2Rad, L = lonDeg * kDeg2Rad;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  E[0] = -sL;      E[1] = cL;       E[2] = 0.0;
  N[0] = -sP * cL; N[1] = -sP * sL; N[2] = cP;
  U[0] = cP * cL;  U[1] = cP * sL;  U[2] = sP;
}

/* Angle difference folded into [-180,180]. The loop form (not fmod) is the one every existing call site
 * used; it is exact for the one-or-two-wrap deltas that actually occur. */
inline double FBWrap180(double deg) {
  while (deg > 180.0) deg -= 360.0;
  while (deg < -180.0) deg += 360.0;
  return deg;
}

/* (lat,lon)'s planar offset from (refLat,refLon): +east / +north, metres. */
inline void FBEnuOffsetM(double refLat, double refLon, double lat, double lon,
                         double &eastM, double &northM) {
  double coslat = std::cos(refLat * kDeg2Rad);
  northM = (lat - refLat) * kMPerDeg;
  eastM = FBWrap180(lon - refLon) * kMPerDeg * coslat;
}

/* Horizontal distance (m) between two points — sign-free, so either point may be the reference. */
inline double FBPlanarDistM(double refLat, double refLon, double lat, double lon) {
  double e, n;
  FBEnuOffsetM(refLat, refLon, lat, lon, e, n);
  return std::sqrt(e * e + n * n);
}

/* True bearing (deg, 0..360) FROM (refLat,refLon) TO (lat,lon). */
inline double FBBearingDeg(double refLat, double refLon, double lat, double lon) {
  double e, n;
  FBEnuOffsetM(refLat, refLon, lat, lon, e, n);
  double brg = std::atan2(e, n) * kRad2Deg;
  return brg < 0.0 ? brg + 360.0 : brg;
}

/* Line of sight <-> body frame, the pair the sensors and the pilot share. ENU in, body-referenced
 * azimuth/elevation out (+az = right of the nose, +el = above the boresight plane) — the standard
 * NED->body Euler sequence Rx(roll)*Ry(pitch)*Rz(yaw) applied to the offset, i.e. WHAT THE ANTENNA SEES
 * rather than what a map would show. systems/FBRadarSystem::RelativeLos is the one caller of the forward
 * direction; systems/FBPilot's BFM steering is the one caller of the inverse, and they must agree
 * exactly or the pilot would steer at a point the radar reports elsewhere. */
inline void FBEnuToBodyLos(double rollDeg, double pitchDeg, double yawDeg, double eastM, double northM,
                           double upM, double &azDeg, double &elDeg) {
  double N = northM, E = eastM, D = -upM;
  double ph = rollDeg * kDeg2Rad, th = pitchDeg * kDeg2Rad, ps = yawDeg * kDeg2Rad;
  double cph = std::cos(ph), sph = std::sin(ph);
  double cth = std::cos(th), sth = std::sin(th);
  double cps = std::cos(ps), sps = std::sin(ps);
  double xb = cth * cps * N + cth * sps * E - sth * D;
  double yb = (sph * sth * cps - cph * sps) * N + (sph * sth * sps + cph * cps) * E + sph * cth * D;
  double zb = (cph * sth * cps + sph * sps) * N + (cph * sth * sps - sph * cps) * E + cph * cth * D;
  azDeg = std::atan2(yb, xb) * kRad2Deg;
  elDeg = -std::atan2(zb, std::sqrt(xb * xb + yb * yb)) * kRad2Deg;
}

/* The exact inverse: a body-referenced direction back into local ENU, unit length. */
inline void FBBodyLosToEnu(double rollDeg, double pitchDeg, double yawDeg, double azDeg, double elDeg,
                           double &eastM, double &northM, double &upM) {
  double ph = rollDeg * kDeg2Rad, th = pitchDeg * kDeg2Rad, ps = yawDeg * kDeg2Rad;
  double cph = std::cos(ph), sph = std::sin(ph);
  double cth = std::cos(th), sth = std::sin(th);
  double cps = std::cos(ps), sps = std::sin(ps);
  double ca = std::cos(azDeg * kDeg2Rad), sa = std::sin(azDeg * kDeg2Rad);
  double ce = std::cos(elDeg * kDeg2Rad), se = std::sin(elDeg * kDeg2Rad);
  double xb = ce * ca, yb = ce * sa, zb = -se;
  northM = cth * cps * xb + (sph * sth * cps - cph * sps) * yb + (cph * sth * cps + sph * sps) * zb;
  eastM = cth * sps * xb + (sph * sth * sps + cph * cps) * yb + (cph * sth * sps - sph * cps) * zb;
  upM = -(-sth * xb + sph * cth * yb + cph * cth * zb);
}

/* A BODY-frame vector (+forward/+right/+down, any unit) expressed in local ENU, same unit. Built on
 * FBBodyLosToEnu rather than on a second copy of the Euler sequence — two spellings of the same
 * rotation drifting apart is precisely the class of bug this file exists to prevent. The one caller is
 * the store-release geometry (app/FBMissionBoot.h): a pylon offset and the rotational velocity at that
 * pylon are both body vectors that have to land in the world's frame. */
inline void FBBodyVecToEnu(double rollDeg, double pitchDeg, double yawDeg, double fwd, double right,
                           double down, double &eastM, double &northM, double &upM) {
  double mag = std::sqrt(fwd * fwd + right * right + down * down);
  if (mag <= 0.0) { eastM = northM = upM = 0.0; return; }
  double azDeg = std::atan2(right, fwd) * kRad2Deg;
  double elDeg = -std::atan2(down, std::sqrt(fwd * fwd + right * right)) * kRad2Deg;
  FBBodyLosToEnu(rollDeg, pitchDeg, yawDeg, azDeg, elDeg, eastM, northM, upM);
  eastM *= mag; northM *= mag; upM *= mag;
}

/* Along/across-track projection of (lat,lon) onto the line through (refLat,refLon) on true heading
 * `courseDeg`: +along down the course from the reference, +across to its right. The runway-axis
 * primitive FBMissionMonitor's on-runway gate, FBRunwayPlateauElevation's footprint, FBPilot's
 * centreline steering and FBAutopilot's localizer all need — one definition, so "on the line" means the
 * same thing to the pilot flying it and to the monitor judging it. */
inline void FBTrackProjectM(double refLat, double refLon, double courseDeg, double lat, double lon,
                            double &alongM, double &acrossM) {
  double e, n;
  FBEnuOffsetM(refLat, refLon, lat, lon, e, n);
  double c = courseDeg * kDeg2Rad;
  alongM = e * std::sin(c) + n * std::cos(c);
  acrossM = e * std::cos(c) - n * std::sin(c);
}

} // namespace FlightBox
#endif
