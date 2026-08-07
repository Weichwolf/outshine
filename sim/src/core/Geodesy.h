/* The ONE planar-ENU geodesy the simulator measures with. Header-only.
 * CONVENTION: the reference point comes FIRST and owns the cosine — EnuOffsetM(ref, p) is p's offset
 * FROM ref, longitude scaled at the REFERENCE latitude, so a bearing and a distance computed by two
 * subsystems agree. Longitude WRAPPING is part of the primitive, not something a caller must remember.
 * SCOPE: deliberately planar/small-angle — tens of nautical miles, not intercontinental.
 * doc/core.md, Abschnitt 10.1. */
#ifndef GEODESY_H
#define GEODESY_H

#include <cmath>
#include "Units.h"

namespace outshine {

/* WGS84 geodetic -> ECEF (m). The one function here that is NOT small-angle: the exact ellipsoid
 * conversion the renderer's camera-relative ECEF world stands on. */
inline void GeoToEcef(double latDeg, double lonDeg, double altM, double out[3]) {
  const double a = 6378137.0, e2 = 6.69437999014e-3;
  double lat = latDeg * kDeg2Rad, lon = lonDeg * kDeg2Rad;
  double sl = std::sin(lat), cl = std::cos(lat);
  double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + altM) * cl * std::cos(lon);
  out[1] = (N + altM) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + altM) * sl;
}

/* The local ENU axes at (lat,lon) in ECEF — where every ECEF camera/vector conversion starts. */
inline void EnuAxesEcef(double latDeg, double lonDeg, double E[3], double N[3], double U[3]) {
  double P = latDeg * kDeg2Rad, L = lonDeg * kDeg2Rad;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  E[0] = -sL;      E[1] = cL;       E[2] = 0.0;
  N[0] = -sP * cL; N[1] = -sP * sL; N[2] = cP;
  U[0] = cP * cL;  U[1] = cP * sL;  U[2] = sP;
}

/* Angle folded into [-180,180]. The loop form is exact for the one-or-two-wrap deltas that occur. */
inline double Wrap180(double deg) {
  while (deg > 180.0) deg -= 360.0;
  while (deg < -180.0) deg += 360.0;
  return deg;
}

/* (lat,lon)'s planar offset from (refLat,refLon): +east / +north, metres. */
inline void EnuOffsetM(double refLat, double refLon, double lat, double lon,
                         double &eastM, double &northM) {
  double coslat = std::cos(refLat * kDeg2Rad);
  northM = (lat - refLat) * kMPerDeg;
  eastM = Wrap180(lon - refLon) * kMPerDeg * coslat;
}

/* Horizontal distance (m) between two points — sign-free, so either point may be the reference. */
inline double PlanarDistM(double refLat, double refLon, double lat, double lon) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  return std::sqrt(e * e + n * n);
}

/* True bearing (deg, 0..360) FROM (refLat,refLon) TO (lat,lon). */
inline double BearingDeg(double refLat, double refLon, double lat, double lon) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  double brg = std::atan2(e, n) * kRad2Deg;
  return brg < 0.0 ? brg + 360.0 : brg;
}

/* Line of sight <-> body frame, shared by the sensors and the pilot: ENU in, body azimuth/elevation out
 * (+az right of the nose, +el above the boresight plane) — WHAT THE ANTENNA SEES, not what a map would
 * show. Radar and pilot must agree exactly or one steers where the other reports. */
inline void EnuToBodyLos(double rollDeg, double pitchDeg, double yawDeg, double eastM, double northM,
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
inline void BodyLosToEnu(double rollDeg, double pitchDeg, double yawDeg, double azDeg, double elDeg,
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

/* A BODY vector (+fwd/+right/+down) in local ENU. Built ON BodyLosToEnu, not on a second copy of the
 * Euler sequence — two spellings of one rotation drifting apart is what this file exists to prevent. */
inline void BodyVecToEnu(double rollDeg, double pitchDeg, double yawDeg, double fwd, double right,
                           double down, double &eastM, double &northM, double &upM) {
  double mag = std::sqrt(fwd * fwd + right * right + down * down);
  if (mag <= 0.0) { eastM = northM = upM = 0.0; return; }
  double azDeg = std::atan2(right, fwd) * kRad2Deg;
  double elDeg = -std::atan2(down, std::sqrt(fwd * fwd + right * right)) * kRad2Deg;
  BodyLosToEnu(rollDeg, pitchDeg, yawDeg, azDeg, elDeg, eastM, northM, upM);
  eastM *= mag; northM *= mag; upM *= mag;
}

/* The exact inverse, on EnuToBodyLos for the same one-spelling reason. */
inline void EnuToBodyVec(double rollDeg, double pitchDeg, double yawDeg, double eastM, double northM,
                           double upM, double &fwd, double &right, double &down) {
  double mag = std::sqrt(eastM * eastM + northM * northM + upM * upM);
  if (mag <= 0.0) { fwd = right = down = 0.0; return; }
  double azDeg = 0.0, elDeg = 0.0;
  EnuToBodyLos(rollDeg, pitchDeg, yawDeg, eastM, northM, upM, azDeg, elDeg);
  double ca = std::cos(azDeg * kDeg2Rad), sa = std::sin(azDeg * kDeg2Rad);
  double ce = std::cos(elDeg * kDeg2Rad), se = std::sin(elDeg * kDeg2Rad);
  fwd = mag * ce * ca;
  right = mag * ce * sa;
  down = -mag * se;
}

/* Along/across-track projection onto the line through the reference on true `courseDeg`: +along down
 * the course, +across to its right. ONE definition, so "on the line" means the same to the pilot flying
 * it and the monitor judging it. */
inline void TrackProjectM(double refLat, double refLon, double courseDeg, double lat, double lon,
                            double &alongM, double &acrossM) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  double c = courseDeg * kDeg2Rad;
  alongM = e * std::sin(c) + n * std::cos(c);
  acrossM = e * std::cos(c) - n * std::sin(c);
}

} // namespace outshine
#endif
