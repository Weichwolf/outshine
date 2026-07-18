/* Where a star actually stands: catalogue coordinates + time + place -> a direction. Pure.
 *
 * This is the arithmetic nothing could check, and the reason it matters was put best by the
 * renderer specialist while we were arguing about how to verify a refactor of it:
 *
 *     "The stars would simply stand over the prime meridian and nobody would notice."
 *
 * A screenshot gate scores GROUND. It has no opinion about the sky, and even a human looking at a
 * night render cannot tell Orion in the right place from Orion an hour or a continent off. So the
 * whole chain — sidereal time, hour angle, the spherical triangle — was unverifiable by
 * construction, sitting inside a render function next to glUniform calls.
 *
 * It is separated here for exactly one reason: Polaris. From latitude phi, the pole star stands at
 * altitude phi, due north, at every hour of every night — the oldest navigation check there is, and
 * it is exact enough to pin the whole transform with one assertion. See test_stars.c.
 *
 * `time(NULL)` stays at the call site. The clock is an input, not a dependency: pass it in and the
 * function is testable at any instant, including ones that have not happened.
 *
 * Angles in degrees. Output is ENU (E=+X, N, up), unit length.
 */
#ifndef W3_STARS_H
#define W3_STARS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  float e, n, u; /* unit direction: east, north, up */
  int above;     /* 0 = at or below the horizon — do not draw, do not trust e/n/u */
} w3_stardir;

/* One catalogue star. `bv` is the B-V colour index (blue-white .. red) for optional tinting. */
typedef struct {
  float ra, dec, mag, bv;
} w3_star;

/* The star catalogue, filled ONCE at startup from the HYG magnitude bands fetched via fb-tiles
 * (cc.c) and decoded by w3_stars_load. Mag-sorted, so a magnitude limit is just a prefix length.
 * NULL until the fetch completes -- no stars render before then (a blank night sky, not a crash).
 * The 44 hand-picked literals that used to live here are gone; this is the real HYG database
 * (~8920 stars to mag 6.5), universal and static, so it is fetched exactly once. */
static w3_star *w3_stars = 0;
static int w3_nstars = 0;

/* Decode the concatenated HYG bands into the catalogue. 6 bytes/star, little-endian, already
 * mag-sorted (band 0 brightest): ra=u16/65536*360deg, dec=i16/32767*90deg, mag=u8/255*8-1.5,
 * bv=u8/255*3-0.5. Pure but for one malloc; returns the star count (0 on empty/failure). */
static int w3_stars_load(const uint8_t *b, int nbytes) {
  int n = nbytes / 6;
  if (n <= 0) return 0;
  w3_star *cat = (w3_star *)malloc((size_t)n * sizeof(w3_star));
  if (!cat) return 0;
  for (int i = 0; i < n; i++) {
    const uint8_t *p = b + i * 6;
    uint16_t ra = (uint16_t)(p[0] | (p[1] << 8));
    int16_t dec = (int16_t)(p[2] | (p[3] << 8));
    cat[i].ra = (float)ra / 65536.0f * 360.0f;
    cat[i].dec = (float)dec / 32767.0f * 90.0f;
    cat[i].mag = (float)p[4] / 255.0f * 8.0f - 1.5f;
    cat[i].bv = (float)p[5] / 255.0f * 3.0f - 0.5f;
  }
  free(w3_stars);
  w3_stars = cat;
  w3_nstars = n;
  return n;
}

/* Greenwich mean sidereal time, degrees, from a Unix timestamp (seconds).
 * The two magic numbers are the standard IAU polynomial at J2000: 280.46061837 deg is GMST at the
 * epoch itself, 360.98564736629 is a sidereal day's worth of rotation per solar day — the ~4-minute
 * drift that makes the same star rise earlier each night. Getting the second one wrong looks right
 * tonight and is an hour off in a fortnight, which is precisely the kind of error nobody sees. */
static double w3_gmst_deg(double unix_seconds) {
  double jd = unix_seconds / 86400.0 + 2440587.5; /* Unix epoch -> Julian Date */
  double dd = jd - 2451545.0;                     /* days since J2000.0 */
  double g = fmod(280.46061837 + 360.98564736629 * dd, 360.0);
  return g < 0 ? g + 360.0 : g;
}

/* Local sidereal time = GMST + longitude east. The star field's entire dependence on WHERE we are
 * enters here and nowhere else. */
static double w3_lst_deg(double gmst_deg, double lon_deg) {
  double l = fmod(gmst_deg + lon_deg, 360.0);
  return l < 0 ? l + 360.0 : l;
}

/* One star: local sidereal time + observer latitude + catalogue RA/dec -> direction, or "below".
 *
 * The horizon cut is at sin(alt) > 0.03 (~1.7 deg), not > 0: a star exactly on the horizon is
 * refracted, dimmed and usually behind terrain, and popping one in and out at the boundary looks
 * like a rendering fault. The margin is deliberate, so it is pinned by a test rather than tuned. */
static w3_stardir w3_star_dir(double lst_deg, double lat_deg, double ra_deg, double dec_deg) {
  const double RAD = M_PI / 180.0;
  w3_stardir d = {0, 0, 0, 0};

  double H = (lst_deg - ra_deg) * RAD; /* hour angle: how far past the meridian */
  double dec = dec_deg * RAD;
  double slat = sin(lat_deg * RAD), clat = cos(lat_deg * RAD);

  double sinAlt = slat * sin(dec) + clat * cos(dec) * cos(H);
  if (sinAlt <= 0.03) return d; /* below/at the horizon: `above` stays 0 */

  double az = atan2(-cos(dec) * sin(H), sin(dec) * clat - cos(dec) * slat * cos(H));
  double ca = sqrt(fmax(0.0, 1.0 - sinAlt * sinAlt)); /* cos(alt) */

  d.e = (float)(ca * sin(az));
  d.n = (float)(ca * cos(az));
  d.u = (float)sinAlt;
  d.above = 1;
  return d;
}

#endif /* W3_STARS_H */
