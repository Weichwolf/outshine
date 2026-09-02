#ifndef OUTSHINE_BASE_GEO_GEODESY_H
#define OUTSHINE_BASE_GEO_GEODESY_H

#include <cmath>
#include "Earth.h"
#include "math/Vec3.h"
#include "Units.h"

namespace outshine {

struct EnuAxes {
  Vec3 East;
  Vec3 North;
  Vec3 Up;
};

struct Ellipsoid {
  double SemiMajorM = 0.0;
  double Flattening = 0.0;
};

struct Geodesic {
  double AlongM = 0.0;
  double FromBearingDeg = 0.0;
  double ToBearingDeg = 0.0;
  bool Converged = false;
};

[[nodiscard]] inline Geodesic
GeodesicOn(const LongitudeLatitudeHeight &from, const LongitudeLatitudeHeight &to, Ellipsoid on) {
  const double semiMajorM = on.SemiMajorM;
  const double flattening = on.Flattening;
  const double fromLatDeg = from.LatitudeDeg;
  const double fromLonDeg = from.LongitudeDeg;
  const double toLatDeg = to.LatitudeDeg;
  const double toLonDeg = to.LongitudeDeg;
  constexpr int kMostTurns = 200;
  constexpr double kSettledRad = kParallelCross;

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

inline void GeoToEcef(const LongitudeLatitudeHeight &at, Vec3 &out) {
  const double latDeg = at.LatitudeDeg;
  const double lonDeg = at.LongitudeDeg;
  const double altM = at.HeightM;
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

[[nodiscard]] inline EnuAxes EnuAxesEcef(const LongitudeLatitudeHeight &at) {
  Vec3 E;
  Vec3 N;
  Vec3 U;
  const double latDeg = at.LatitudeDeg;
  const double lonDeg = at.LongitudeDeg;
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
  return {.East = E, .North = N, .Up = U};
}

inline double Wrap180(double deg) {
  while (deg > kDegPerHalfTurn) { deg -= kDegPerTurn; }
  while (deg < -kDegPerHalfTurn) { deg += kDegPerTurn; }
  return deg;
}

[[nodiscard]] inline EastNorth EnuOffsetM(const LongitudeLatitudeHeight &from,
                                          const LongitudeLatitudeHeight &at) {
  const double coslat = std::cos(from.LatitudeDeg * kDeg2Rad);
  return {.EastM = Wrap180(at.LongitudeDeg - from.LongitudeDeg) * kMPerDeg * coslat,
          .NorthM = (at.LatitudeDeg - from.LatitudeDeg) * kMPerDeg};
}

[[nodiscard]] inline double PlanarDistM(const LongitudeLatitudeHeight &from,
                                        const LongitudeLatitudeHeight &at) {
  const EastNorth away = EnuOffsetM(from, at);
  return std::hypot(away.EastM, away.NorthM);
}

[[nodiscard]] inline double BearingDeg(const LongitudeLatitudeHeight &from,
                                       const LongitudeLatitudeHeight &at) {
  const EastNorth away = EnuOffsetM(from, at);
  const double brg = std::atan2(away.EastM, away.NorthM) * kRad2Deg;
  return brg < 0.0 ? brg + kDegPerTurn : brg;
}

[[nodiscard]] inline LookDirection EnuToBodyLos(const Attitude &stands, const EastNorthUp &at) {
  const double N = at.NorthM;
  const double E = at.EastM;
  const double D = -at.UpM;
  const double ph = stands.RollDeg * kDeg2Rad;
  const double th = stands.PitchDeg * kDeg2Rad;
  const double ps = stands.YawDeg * kDeg2Rad;
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
  return {.AzimuthDeg = std::atan2(yb, xb) * kRad2Deg,
          .ElevationDeg = -std::atan2(zb, std::hypot(xb, yb)) * kRad2Deg};
}

[[nodiscard]] inline EastNorthUp BodyLosToEnu(const Attitude &stands, const LookDirection &sees) {
  const double ph = stands.RollDeg * kDeg2Rad;
  const double th = stands.PitchDeg * kDeg2Rad;
  const double ps = stands.YawDeg * kDeg2Rad;
  const double cph = std::cos(ph);
  const double sph = std::sin(ph);
  const double cth = std::cos(th);
  const double sth = std::sin(th);
  const double cps = std::cos(ps);
  const double sps = std::sin(ps);
  const double ca = std::cos(sees.AzimuthDeg * kDeg2Rad);
  const double sa = std::sin(sees.AzimuthDeg * kDeg2Rad);
  const double ce = std::cos(sees.ElevationDeg * kDeg2Rad);
  const double se = std::sin(sees.ElevationDeg * kDeg2Rad);
  const double xb = ce * ca;
  const double yb = ce * sa;
  const double zb = -se;
  return {.EastM = cth * sps * xb + (sph * sth * sps + cph * cps) * yb +
                   (cph * sth * sps - sph * cps) * zb,
          .NorthM = cth * cps * xb + (sph * sth * cps - cph * sps) * yb +
                    (cph * sth * cps + sph * sps) * zb,
          .UpM = -(-sth * xb + sph * cth * yb + cph * cth * zb)};
}

[[nodiscard]] inline EastNorthUp BodyVecToEnu(const Attitude &stands, const Vec3 &body) {
  const double mag = Length(body);
  if (mag <= 0.0) { return {}; }
  const LookDirection sees{.AzimuthDeg = std::atan2(body[1], body[0]) * kRad2Deg,
                           .ElevationDeg =
                               -std::atan2(body[2], std::hypot(body[0], body[1])) * kRad2Deg};
  const EastNorthUp unit = BodyLosToEnu(stands, sees);
  return {.EastM = unit.EastM * mag, .NorthM = unit.NorthM * mag, .UpM = unit.UpM * mag};
}

[[nodiscard]] inline Vec3 EnuToBodyVec(const Attitude &stands, const EastNorthUp &at) {
  const double mag = std::sqrt(at.EastM * at.EastM + at.NorthM * at.NorthM + at.UpM * at.UpM);
  if (mag <= 0.0) { return {}; }
  const LookDirection sees = EnuToBodyLos(stands, at);
  const double ca = std::cos(sees.AzimuthDeg * kDeg2Rad);
  const double sa = std::sin(sees.AzimuthDeg * kDeg2Rad);
  const double ce = std::cos(sees.ElevationDeg * kDeg2Rad);
  const double se = std::sin(sees.ElevationDeg * kDeg2Rad);
  return {{mag * ce * ca, mag * ce * sa, -mag * se}};
}

} // namespace outshine
#endif
