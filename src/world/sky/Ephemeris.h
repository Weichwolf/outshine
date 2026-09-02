#ifndef OUTSHINE_WORLD_SKY_EPHEMERIS_H
#define OUTSHINE_WORLD_SKY_EPHEMERIS_H
#include <algorithm>
#include <cmath>
#include "Earth.h"
#include "State.h"
#include "Units.h"

namespace outshine {

constexpr int kEphemerisMinYear = 1901, kEphemerisMaxYear = 2099;

struct Lunar {
  LookDirection Sees;
  double Phase = 0.0;
};

[[nodiscard]] inline LookDirection EarthSunPos(LongitudeLatitude at, double utcS) {
  const double D2R = kDeg2Rad;
  const double jd = utcS / 86400.0 + 2440587.5;
  const double n = jd - 2451545.0;
  const double L = fmod(280.460 + 0.9856474 * n, kDegPerTurn);
  const double g = fmod(357.528 + 0.9856003 * n, kDegPerTurn) * D2R;
  const double lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * D2R;
  const double eps = (23.439 - 0.0000004 * n) * D2R;
  const double dec = asin(sin(eps) * sin(lam));
  const double ra = atan2(cos(eps) * sin(lam), cos(lam));
  const double gmst = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
  const double lst = fmod(gmst * 15.0 + at.LongitudeDeg, kDegPerTurn);
  const double ha = (lst - ra / D2R) * D2R;
  const double la = at.LatitudeDeg * D2R;
  const double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  return {.AzimuthDeg =
              fmod(atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + kDegPerTurn,
                   kDegPerTurn),
          .ElevationDeg = asin(sinel) / D2R};
}

[[nodiscard]] inline Lunar EarthMoonPos(LongitudeLatitude at, double utcS) {
  const double D2R = kDeg2Rad;
  const double jd = utcS / 86400.0 + 2440587.5;
  const double d = jd - 2451543.5;

  const double N = fmod(125.1228 - 0.0529538083 * d, kDegPerTurn) * D2R;
  const double i = 5.1454 * D2R;
  const double w = fmod(318.0634 + 0.1643573223 * d, kDegPerTurn) * D2R;
  const double a = 60.2666;
  const double e = 0.054900;
  const double M = fmod(115.3654 + 13.0649929509 * d, kDegPerTurn) * D2R;

  double E = M + e * sin(M) * (1.0 + e * cos(M));
  for (int k = 0; k < 4; k++) { E -= (E - e * sin(E) - M) / (1.0 - e * cos(E)); }

  const double xv = a * (cos(E) - e);
  const double yv = a * (sqrt(1.0 - e * e) * sin(E));
  const double v = atan2(yv, xv);
  const double r = sqrt(xv * xv + yv * yv);

  const double xh = r * (cos(N) * cos(v + w) - sin(N) * sin(v + w) * cos(i));
  const double yh = r * (sin(N) * cos(v + w) + cos(N) * sin(v + w) * cos(i));
  const double zh = r * sin(v + w) * sin(i);
  const double lonecl = atan2(yh, xh);
  const double latecl = atan2(zh, sqrt(xh * xh + yh * yh));

  const double ecl = (23.4393 - 3.563e-7 * d) * D2R;
  const double xg = cos(lonecl) * cos(latecl);
  const double yg = sin(lonecl) * cos(latecl);
  const double zg = sin(latecl);
  const double xe = xg;
  const double ye = yg * cos(ecl) - zg * sin(ecl);
  const double ze = yg * sin(ecl) + zg * cos(ecl);
  const double ra = atan2(ye, xe);
  const double dec = atan2(ze, sqrt(xe * xe + ye * ye));

  const double n = jd - 2451545.0;
  const double gmst = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
  const double lst = fmod(gmst * 15.0 + at.LongitudeDeg, kDegPerTurn);
  const double ha = (lst - ra / D2R) * D2R;
  const double la = at.LatitudeDeg * D2R;
  const double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  const double Lsun = fmod(280.460 + 0.9856474 * n, kDegPerTurn) * D2R;
  const double elong = lonecl - Lsun;
  return {.Sees = {.AzimuthDeg = fmod(
                       atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + kDegPerTurn,
                       kDegPerTurn),
                   .ElevationDeg = asin(sinel) / D2R},
          .Phase = (1.0 - cos(elong)) * 0.5};
}

struct Solar {
  float SunElDeg = 0.0f, SunAzDeg = 0.0f;
  float MoonElDeg = 0.0f, MoonAzDeg = 0.0f, MoonPhase = 0.0f;
};

[[nodiscard]] inline Solar SolarAt(LongitudeLatitude at, double utcS) {
  const LookDirection sun = EarthSunPos(at, utcS);
  const Lunar moon = EarthMoonPos(at, utcS);
  return {.SunElDeg = static_cast<float>(sun.ElevationDeg),
          .SunAzDeg = static_cast<float>(sun.AzimuthDeg),
          .MoonElDeg = static_cast<float>(moon.Sees.ElevationDeg),
          .MoonAzDeg = static_cast<float>(moon.Sees.AzimuthDeg),
          .MoonPhase = static_cast<float>(moon.Phase)};
}

inline void SolarToEnv(const Solar &s, State &st) {
  st.Env.SunElDeg = s.SunElDeg;
  st.Env.SunAzDeg = s.SunAzDeg;
  st.Env.MoonElDeg = s.MoonElDeg;
  st.Env.MoonAzDeg = s.MoonAzDeg;
  st.Env.MoonPhase = s.MoonPhase;
  st.Env.H.Publish(st.NowS);
}

inline double DaylightFactor(double sunElDeg) {
  const double t = std::clamp((sunElDeg + 9.0) / 12.0, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

} // namespace outshine
#endif
