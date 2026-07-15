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

typedef struct {
    float e, n, u;   /* unit direction: east, north, up */
    int   above;     /* 0 = at or below the horizon — do not draw, do not trust e/n/u */
} w3_stardir;

/* Greenwich mean sidereal time, degrees, from a Unix timestamp (seconds).
 * The two magic numbers are the standard IAU polynomial at J2000: 280.46061837 deg is GMST at the
 * epoch itself, 360.98564736629 is a sidereal day's worth of rotation per solar day — the ~4-minute
 * drift that makes the same star rise earlier each night. Getting the second one wrong looks right
 * tonight and is an hour off in a fortnight, which is precisely the kind of error nobody sees. */
static double w3_gmst_deg(double unix_seconds)
{
    double jd = unix_seconds / 86400.0 + 2440587.5;   /* Unix epoch -> Julian Date */
    double dd = jd - 2451545.0;                       /* days since J2000.0 */
    double g  = fmod(280.46061837 + 360.98564736629 * dd, 360.0);
    return g < 0 ? g + 360.0 : g;
}

/* Local sidereal time = GMST + longitude east. The star field's entire dependence on WHERE we are
 * enters here and nowhere else. */
static double w3_lst_deg(double gmst_deg, double lon_deg)
{
    double l = fmod(gmst_deg + lon_deg, 360.0);
    return l < 0 ? l + 360.0 : l;
}

/* One star: local sidereal time + observer latitude + catalogue RA/dec -> direction, or "below".
 *
 * The horizon cut is at sin(alt) > 0.03 (~1.7 deg), not > 0: a star exactly on the horizon is
 * refracted, dimmed and usually behind terrain, and popping one in and out at the boundary looks
 * like a rendering fault. The margin is deliberate, so it is pinned by a test rather than tuned. */
static w3_stardir w3_star_dir(double lst_deg, double lat_deg, double ra_deg, double dec_deg)
{
    const double RAD = M_PI / 180.0;
    w3_stardir d = {0, 0, 0, 0};

    double H    = (lst_deg - ra_deg) * RAD;    /* hour angle: how far past the meridian */
    double dec  = dec_deg * RAD;
    double slat = sin(lat_deg * RAD), clat = cos(lat_deg * RAD);

    double sinAlt = slat * sin(dec) + clat * cos(dec) * cos(H);
    if (sinAlt <= 0.03) return d;              /* below/at the horizon: `above` stays 0 */

    double az = atan2(-cos(dec) * sin(H), sin(dec) * clat - cos(dec) * slat * cos(H));
    double ca = sqrt(fmax(0.0, 1.0 - sinAlt * sinAlt));   /* cos(alt) */

    d.e = (float)(ca * sin(az));
    d.n = (float)(ca * cos(az));
    d.u = (float)sinAlt;
    d.above = 1;
    return d;
}

#endif /* W3_STARS_H */
