#ifndef OUTSHINE_BASE_GEO_GEODESY_H
#define OUTSHINE_BASE_GEO_GEODESY_H

#include <cmath>
#include "math/Vec3.h"
#include "Units.h"

namespace outshine {

struct Geodesic {
  double AlongM = 0.0;
  double FromBearingDeg = 0.0;
  double ToBearingDeg = 0.0;
  bool Converged = false;
};

[[nodiscard]] inline Geodesic GeodesicOn(double fromLatDeg,
                                         double fromLonDeg,
                                         double toLatDeg,
                                         double toLonDeg,
                                         double semiMajorM,
                                         double flattening) {
  constexpr int kMostTurns = 200;
  constexpr double kSettledRad = 1.0e-12;

  Geodesic out;
  const double f = flattening;
  const double b = semiMajorM * (1.0 - f);
  const double u1 = std::atan((1.0 - f) * std::tan(fromLatDeg * kDeg2Rad));
  const double u2 = std::atan((1.0 - f) * std::tan(toLatDeg * kDeg2Rad));
  double apart = (toLonDeg - fromLonDeg) * kDeg2Rad;
  while (apart > kPi) { apart -= 2.0 * kPi; }
  while (apart < -kPi) { apart += 2.0 * kPi; }

  const double sinU1 = std::sin(u1);
  const double cosU1 = std::cos(u1);
  const double sinU2 = std::sin(u2);
  const double cosU2 = std::cos(u2);

  double lambda = apart;
  double sinSigma = 0.0;
  double cosSigma = 1.0;
  double sigma = 0.0;
  double cosSqAlpha = 1.0;
  double cos2SigmaM = 0.0;
  for (int turn = 0; turn < kMostTurns; ++turn) {
    const double sinLambda = std::sin(lambda);
    const double cosLambda = std::cos(lambda);
    const double across = cosU2 * sinLambda;
    const double along = cosU1 * sinU2 - sinU1 * cosU2 * cosLambda;
    sinSigma = std::sqrt(across * across + along * along);
    if (sinSigma == 0.0) {
      out.Converged = true;
      return out;
    }
    cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
    sigma = std::atan2(sinSigma, cosSigma);
    const double sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
    cosSqAlpha = 1.0 - sinAlpha * sinAlpha;
    cos2SigmaM = cosSqAlpha == 0.0 ? 0.0 : cosSigma - 2.0 * sinU1 * sinU2 / cosSqAlpha;
    const double c = f / 16.0 * cosSqAlpha * (4.0 + f * (4.0 - 3.0 * cosSqAlpha));
    const double was = lambda;
    lambda = apart +
             (1.0 - c) * f * sinAlpha *
                 (sigma + c * sinSigma *
                              (cos2SigmaM + c * cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)));
    if (std::fabs(lambda - was) < kSettledRad) {
      out.Converged = true;
      break;
    }
  }

  if (!out.Converged) { return out; }

  const double uSq = cosSqAlpha * (semiMajorM * semiMajorM - b * b) / (b * b);
  const double a = 1.0 + uSq / 16384.0 * (4096.0 + uSq * (-768.0 + uSq * (320.0 - 175.0 * uSq)));
  const double bb = uSq / 1024.0 * (256.0 + uSq * (-128.0 + uSq * (74.0 - 47.0 * uSq)));
  const double deltaSigma =
      bb * sinSigma *
      (cos2SigmaM + bb / 4.0 *
                        (cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM) -
                         bb / 6.0 * cos2SigmaM * (-3.0 + 4.0 * sinSigma * sinSigma) *
                             (-3.0 + 4.0 * cos2SigmaM * cos2SigmaM)));
  out.AlongM = b * a * (sigma - deltaSigma);

  const double sinLambda = std::sin(lambda);
  const double cosLambda = std::cos(lambda);
  out.FromBearingDeg =
      std::atan2(cosU2 * sinLambda, cosU1 * sinU2 - sinU1 * cosU2 * cosLambda) / kDeg2Rad;
  out.ToBearingDeg =
      std::atan2(cosU1 * sinLambda, -sinU1 * cosU2 + cosU1 * sinU2 * cosLambda) / kDeg2Rad;
  return out;
}

inline void GeoToEcef(double latDeg, double lonDeg, double altM, Vec3 &out) {
  const double a = 6378137.0;
  const double e2 = 6.69437999014e-3;
  const double lat = latDeg * kDeg2Rad;
  const double lon = lonDeg * kDeg2Rad;
  const double sl = std::sin(lat);
  const double cl = std::cos(lat);
  const double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + altM) * cl * std::cos(lon);
  out[1] = (N + altM) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + altM) * sl;
}

inline void EnuAxesEcef(double latDeg, double lonDeg, Vec3 &E, Vec3 &N, Vec3 &U) {
  const double P = latDeg * kDeg2Rad;
  const double L = lonDeg * kDeg2Rad;
  const double sP = std::sin(P);
  const double cP = std::cos(P);
  const double sL = std::sin(L);
  const double cL = std::cos(L);
  E[0] = -sL;
  E[1] = cL;
  E[2] = 0.0;
  N[0] = -sP * cL;
  N[1] = -sP * sL;
  N[2] = cP;
  U[0] = cP * cL;
  U[1] = cP * sL;
  U[2] = sP;
}

inline double Wrap180(double deg) {
  while (deg > 180.0) { deg -= 360.0; }
  while (deg < -180.0) { deg += 360.0; }
  return deg;
}

inline void
EnuOffsetM(double refLat, double refLon, double lat, double lon, double &eastM, double &northM) {
  const double coslat = std::cos(refLat * kDeg2Rad);
  northM = (lat - refLat) * kMPerDeg;
  eastM = Wrap180(lon - refLon) * kMPerDeg * coslat;
}

inline double PlanarDistM(double refLat, double refLon, double lat, double lon) {
  double e;
  double n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  return std::sqrt(e * e + n * n);
}

inline double BearingDeg(double refLat, double refLon, double lat, double lon) {
  double e;
  double n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  const double brg = std::atan2(e, n) * kRad2Deg;
  return brg < 0.0 ? brg + 360.0 : brg;
}

inline void EnuToBodyLos(double rollDeg,
                         double pitchDeg,
                         double yawDeg,
                         double eastM,
                         double northM,
                         double upM,
                         double &azDeg,
                         double &elDeg) {
  const double N = northM;
  const double E = eastM;
  const double D = -upM;
  const double ph = rollDeg * kDeg2Rad;
  const double th = pitchDeg * kDeg2Rad;
  const double ps = yawDeg * kDeg2Rad;
  const double cph = std::cos(ph);
  const double sph = std::sin(ph);
  const double cth = std::cos(th);
  const double sth = std::sin(th);
  const double cps = std::cos(ps);
  const double sps = std::sin(ps);
  const double xb = cth * cps * N + cth * sps * E - sth * D;
  const double yb =
      (sph * sth * cps - cph * sps) * N + (sph * sth * sps + cph * cps) * E + sph * cth * D;
  const double zb =
      (cph * sth * cps + sph * sps) * N + (cph * sth * sps - sph * cps) * E + cph * cth * D;
  azDeg = std::atan2(yb, xb) * kRad2Deg;
  elDeg = -std::atan2(zb, std::sqrt(xb * xb + yb * yb)) * kRad2Deg;
}

inline void BodyLosToEnu(double rollDeg,
                         double pitchDeg,
                         double yawDeg,
                         double azDeg,
                         double elDeg,
                         double &eastM,
                         double &northM,
                         double &upM) {
  const double ph = rollDeg * kDeg2Rad;
  const double th = pitchDeg * kDeg2Rad;
  const double ps = yawDeg * kDeg2Rad;
  const double cph = std::cos(ph);
  const double sph = std::sin(ph);
  const double cth = std::cos(th);
  const double sth = std::sin(th);
  const double cps = std::cos(ps);
  const double sps = std::sin(ps);
  const double ca = std::cos(azDeg * kDeg2Rad);
  const double sa = std::sin(azDeg * kDeg2Rad);
  const double ce = std::cos(elDeg * kDeg2Rad);
  const double se = std::sin(elDeg * kDeg2Rad);
  const double xb = ce * ca;
  const double yb = ce * sa;
  const double zb = -se;
  northM = cth * cps * xb + (sph * sth * cps - cph * sps) * yb + (cph * sth * cps + sph * sps) * zb;
  eastM = cth * sps * xb + (sph * sth * sps + cph * cps) * yb + (cph * sth * sps - sph * cps) * zb;
  upM = -(-sth * xb + sph * cth * yb + cph * cth * zb);
}

inline void BodyVecToEnu(double rollDeg,
                         double pitchDeg,
                         double yawDeg,
                         double fwd,
                         double right,
                         double down,
                         double &eastM,
                         double &northM,
                         double &upM) {
  const double mag = std::sqrt(fwd * fwd + right * right + down * down);
  if (mag <= 0.0) {
    eastM = northM = upM = 0.0;
    return;
  }
  const double azDeg = std::atan2(right, fwd) * kRad2Deg;
  const double elDeg = -std::atan2(down, std::sqrt(fwd * fwd + right * right)) * kRad2Deg;
  BodyLosToEnu(rollDeg, pitchDeg, yawDeg, azDeg, elDeg, eastM, northM, upM);
  eastM *= mag;
  northM *= mag;
  upM *= mag;
}

inline void EnuToBodyVec(double rollDeg,
                         double pitchDeg,
                         double yawDeg,
                         double eastM,
                         double northM,
                         double upM,
                         double &fwd,
                         double &right,
                         double &down) {
  const double mag = std::sqrt(eastM * eastM + northM * northM + upM * upM);
  if (mag <= 0.0) {
    fwd = right = down = 0.0;
    return;
  }
  double azDeg = 0.0;
  double elDeg = 0.0;
  EnuToBodyLos(rollDeg, pitchDeg, yawDeg, eastM, northM, upM, azDeg, elDeg);
  const double ca = std::cos(azDeg * kDeg2Rad);
  const double sa = std::sin(azDeg * kDeg2Rad);
  const double ce = std::cos(elDeg * kDeg2Rad);
  const double se = std::sin(elDeg * kDeg2Rad);
  fwd = mag * ce * ca;
  right = mag * ce * sa;
  down = -mag * se;
}

inline void TrackProjectM(double refLat,
                          double refLon,
                          double courseDeg,
                          double lat,
                          double lon,
                          double &alongM,
                          double &acrossM) {
  double e;
  double n;
  EnuOffsetM(refLat, refLon, lat, lon, e, n);
  const double c = courseDeg * kDeg2Rad;
  alongM = e * std::sin(c) + n * std::cos(c);
  acrossM = e * std::cos(c) - n * std::sin(c);
}

} // namespace outshine
#endif
