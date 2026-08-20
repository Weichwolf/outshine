#ifndef GEODESY_H
#define GEODESY_H

#include <cmath>
#include "Units.h"

namespace outshine {

inline void GeoToEcef(double latDeg, double lonDeg, double altM, double out[3]) {
  const double a = 6378137.0, e2 = 6.69437999014e-3;
  double lat = latDeg * kDeg2Rad, lon = lonDeg * kDeg2Rad;
  double sl = std::sin(lat), cl = std::cos(lat);
  double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + altM) * cl * std::cos(lon);
  out[1] = (N + altM) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + altM) * sl;
}

inline void EnuAxesEcef(double latDeg, double lonDeg, double E[3], double N[3], double U[3]) {
  double P = latDeg * kDeg2Rad, L = lonDeg * kDeg2Rad;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  E[0] = -sL;      E[1] = cL;       E[2] = 0.0;
  N[0] = -sP * cL; N[1] = -sP * sL; N[2] = cP;
  U[0] = cP * cL;  U[1] = cP * sL;  U[2] = sP;
}

inline double Wrap180(double deg) {
  while (deg > 180.0) deg -= 360.0;
  while (deg < -180.0) deg += 360.0;
  return deg;
}

inline void EnuOffsetM(double refLat, double refLon, double lat, double lon,
                         double &eastM, double &northM) {
  double coslat = std::cos(refLat * kDeg2Rad);
  northM = (lat - refLat) * kMPerDeg;
  eastM = Wrap180(lon - refLon) * kMPerDeg * coslat;
}

inline double PlanarDistM(double refLat, double refLon, double lat, double lon) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  return std::sqrt(e * e + n * n);
}

inline double BearingDeg(double refLat, double refLon, double lat, double lon) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  double brg = std::atan2(e, n) * kRad2Deg;
  return brg < 0.0 ? brg + 360.0 : brg;
}

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

inline void BodyVecToEnu(double rollDeg, double pitchDeg, double yawDeg, double fwd, double right,
                           double down, double &eastM, double &northM, double &upM) {
  double mag = std::sqrt(fwd * fwd + right * right + down * down);
  if (mag <= 0.0) { eastM = northM = upM = 0.0; return; }
  double azDeg = std::atan2(right, fwd) * kRad2Deg;
  double elDeg = -std::atan2(down, std::sqrt(fwd * fwd + right * right)) * kRad2Deg;
  BodyLosToEnu(rollDeg, pitchDeg, yawDeg, azDeg, elDeg, eastM, northM, upM);
  eastM *= mag; northM *= mag; upM *= mag;
}

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

inline void TrackProjectM(double refLat, double refLon, double courseDeg, double lat, double lon,
                            double &alongM, double &acrossM) {
  double e, n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  double c = courseDeg * kDeg2Rad;
  alongM = e * std::sin(c) + n * std::cos(c);
  acrossM = e * std::cos(c) - n * std::sin(c);
}

}
#endif
