/* FlightBox — sun/moon ephemeris: pure functions, no state, no globals.
 *
 * SunPos is a verbatim port of flightbox/server.c's sun_pos (low-precision NOAA-style solar
 * formulae, < ~0.5 deg) — the legacy WS/UDP path fed the HUD/sky from it; the in-process sim
 * (flightsim.h) never got it, so EVS was frozen at a hardcoded "always noon, no moon" sky
 * regardless of FB_SIM_UTC (sim-critic gate finding).
 *
 * MoonPos/MoonPhase are NEW — server.c never computed the moon at all (never had more than a
 * hardcoded moon_el/az/phase either). This is a fresh port of Paul Schlyter's well-known
 * low-precision geocentric lunar-position formulae
 * (http://www.stjarnhimlen.se/comp/ppcomp.html, "Position of the Moon" — public domain, deliberately
 * WITHOUT his long perturbation-term table): good to roughly a degree, plenty for a sky disc +
 * phase, not for real lunar navigation. Phase fraction uses the standard (1-cos elongation)/2
 * approximation from the sun-moon ecliptic-longitude difference. */
#ifndef FB_EPHEMERIS_H
#define FB_EPHEMERIS_H
#include <cmath>
#include <ctime>

namespace FlightBox {

/* Real solar elevation/azimuth (deg) at (lat,lon) for a UTC epoch. */
static inline void SunPos(double lat, double lon, time_t utc, float *el, float *az) {
  double D2R = M_PI / 180.0, jd = utc / 86400.0 + 2440587.5, n = jd - 2451545.0;
  double L = fmod(280.460 + 0.9856474 * n, 360.0), g = fmod(357.528 + 0.9856003 * n, 360.0) * D2R;
  double lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * D2R, eps = (23.439 - 0.0000004 * n) * D2R;
  double dec = asin(sin(eps) * sin(lam)), ra = atan2(cos(eps) * sin(lam), cos(lam));
  double gmst = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
  double lst = fmod(gmst * 15.0 + lon, 360.0), ha = (lst - ra / D2R) * D2R, la = lat * D2R;
  double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  *el = (float)(asin(sinel) / D2R);
  *az = (float)(fmod(atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + 360.0, 360.0));
}

/* Geocentric Moon elevation/azimuth (deg) + illuminated phase fraction (0=new..1=full). Same
 * (lat,lon,utc) convention as SunPos so both read off the identical time base. */
static inline void MoonPos(double lat, double lon, time_t utc, float *el, float *az, float *phase) {
  double D2R = M_PI / 180.0, jd = utc / 86400.0 + 2440587.5;
  double d = jd - 2451543.5;   /* days since Schlyter's epoch (1999-12-31 00:00 UT) */

  /* Moon orbital elements (deg), linear in d (Schlyter, simplified — no secular/periodic terms) */
  double N = fmod(125.1228 - 0.0529538083 * d, 360.0) * D2R;   /* long. of ascending node */
  double i = 5.1454 * D2R;                                      /* inclination */
  double w = fmod(318.0634 + 0.1643573223 * d, 360.0) * D2R;    /* argument of perigee */
  double a = 60.2666, e = 0.054900;                             /* semi-major axis (Earth radii), eccentricity */
  double M = fmod(115.3654 + 13.0649929509 * d, 360.0) * D2R;   /* mean anomaly */

  double E = M + e * sin(M) * (1.0 + e * cos(M));   /* Kepler's equation, Newton-refined */
  for (int k = 0; k < 4; k++) E -= (E - e * sin(E) - M) / (1.0 - e * cos(E));

  double xv = a * (cos(E) - e), yv = a * (sqrt(1.0 - e * e) * sin(E));
  double v = atan2(yv, xv), r = sqrt(xv * xv + yv * yv);   /* true anomaly, distance */

  double xh = r * (cos(N) * cos(v + w) - sin(N) * sin(v + w) * cos(i));
  double yh = r * (sin(N) * cos(v + w) + cos(N) * sin(v + w) * cos(i));
  double zh = r * (sin(v + w) * sin(i));
  double lonecl = atan2(yh, xh), latecl = atan2(zh, sqrt(xh * xh + yh * yh));   /* geocentric ecliptic */

  double ecl = (23.4393 - 3.563e-7 * d) * D2R;   /* obliquity of the ecliptic */
  double xg = cos(lonecl) * cos(latecl), yg = sin(lonecl) * cos(latecl), zg = sin(latecl);
  double xe = xg, ye = yg * cos(ecl) - zg * sin(ecl), ze = yg * sin(ecl) + zg * cos(ecl);
  double ra = atan2(ye, xe), dec = atan2(ze, sqrt(xe * xe + ye * ye));   /* equatorial */

  double n = jd - 2451545.0;
  double gmst = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
  double lst = fmod(gmst * 15.0 + lon, 360.0), ha = (lst - ra / D2R) * D2R, la = lat * D2R;
  double sinel = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
  *el = (float)(asin(sinel) / D2R);
  *az = (float)(fmod(atan2(-sin(ha), tan(dec) * cos(la) - sin(la) * cos(ha)) / D2R + 360.0, 360.0));

  /* Phase from the sun-moon ecliptic-longitude elongation (Sun's mean ecliptic longitude, SunPos's
   * own L formula, same time base n) -- (1-cos elong)/2, the standard low-precision approximation. */
  double Lsun = fmod(280.460 + 0.9856474 * n, 360.0) * D2R;
  double elong = lonecl - Lsun;
  *phase = (float)((1.0 - cos(elong)) * 0.5);
}

} // namespace FlightBox
#endif /* FB_EPHEMERIS_H */
