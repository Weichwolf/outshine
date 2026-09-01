#ifndef OUTSHINE_WORLD_SKY_EPHEMERIS_H
#define OUTSHINE_WORLD_SKY_EPHEMERIS_H
#include <algorithm>
#include <cmath>
#include "State.h"
#include "Units.h"

namespace outshine {

constexpr int kEphemerisMinYear = 1901, kEphemerisMaxYear = 2099;

inline void EarthSunPos(double lat, double lon, double utc, float *el, float *az) {
  const double D2R = kDeg2Rad;
  const double jd = utc / 86400.0 + 2440587.5;
  const double n = jd - 2451545.0;
  const double L = fmod(280.460 + 0.9856474 * n, 360.0);
  const double g = fmod(357.528 + 0.9856003 * n, 360.0) * D2R;
  const double lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * D2R;
  const double eps = (23.439 - 0.0000004 * n) * D2R;
  const double dec = asin(sin(eps) * sin(lam));
  const double ra = atan2(cos(eps) * sin(lam), cos(lam));
  const double gmst = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
  const double lst = fmod(gmst * 15.0 + lon, 360.0);
  const double ha = (lst - ra / D2R) * D2R;
  const double la = lat * D2R;
  const double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  *el = static_cast<float>(asin(sinel) / D2R);
  *az = static_cast<float>(
      fmod(atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + 360.0, 360.0));
}

inline void EarthMoonPos(double lat, double lon, double utc, float *el, float *az, float *phase) {
  const double D2R = kDeg2Rad;
  const double jd = utc / 86400.0 + 2440587.5;
  const double d = jd - 2451543.5;

  const double N = fmod(125.1228 - 0.0529538083 * d, 360.0) * D2R;
  const double i = 5.1454 * D2R;
  const double w = fmod(318.0634 + 0.1643573223 * d, 360.0) * D2R;
  const double a = 60.2666;
  const double e = 0.054900;
  const double M = fmod(115.3654 + 13.0649929509 * d, 360.0) * D2R;

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
  const double lst = fmod(gmst * 15.0 + lon, 360.0);
  const double ha = (lst - ra / D2R) * D2R;
  const double la = lat * D2R;
  const double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  *el = static_cast<float>(asin(sinel) / D2R);
  *az = static_cast<float>(
      fmod(atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + 360.0, 360.0));

  const double Lsun = fmod(280.460 + 0.9856474 * n, 360.0) * D2R;
  const double elong = lonecl - Lsun;
  *phase = static_cast<float>((1.0 - cos(elong)) * 0.5);
}

struct Solar {
  float SunElDeg = 0.0f, SunAzDeg = 0.0f;
  float MoonElDeg = 0.0f, MoonAzDeg = 0.0f, MoonPhase = 0.0f;
};

inline Solar SolarAt(double lat, double lon, double utc) {
  Solar s;
  EarthSunPos(lat, lon, utc, &s.SunElDeg, &s.SunAzDeg);
  EarthMoonPos(lat, lon, utc, &s.MoonElDeg, &s.MoonAzDeg, &s.MoonPhase);
  return s;
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
