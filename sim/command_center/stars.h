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

/* ~45 brightest stars: right ascension (deg), declination (deg), visual magnitude.
 * Rendered at their TRUE alt/az for the origin + wall-clock time, so the real
 * constellations (Orion, Big Dipper, Cassiopeia, ...) appear where they actually are.
 * Embedded: 528 B of natural constant, not an asset to fetch. */
typedef struct { float ra, dec, mag; } w3_star;
static const w3_star W3_STARS[] = {
 {101.29f,-16.72f,-1.46f},{95.99f,-52.70f,-0.74f},{219.90f,-60.83f,-0.27f},{213.92f,19.18f,-0.05f},
 {279.23f,38.78f,0.03f},{79.17f,46.00f,0.08f},{78.63f,-8.20f,0.13f},{114.83f,5.22f,0.34f},
 {88.79f,7.41f,0.50f},{24.43f,-57.24f,0.46f},{210.96f,-60.37f,0.61f},{297.70f,8.87f,0.77f},
 {186.65f,-63.10f,0.77f},{68.98f,16.51f,0.85f},{201.30f,-11.16f,0.98f},{247.35f,-26.43f,1.09f},
 {116.33f,28.03f,1.14f},{344.41f,-29.62f,1.16f},{310.36f,45.28f,1.25f},{191.93f,-59.69f,1.25f},
 {152.09f,11.97f,1.35f},{104.66f,-28.97f,1.50f},{113.65f,31.89f,1.58f},{263.40f,-37.10f,1.62f},
 {81.28f,6.35f,1.64f},{81.57f,28.61f,1.65f},{84.05f,-1.20f,1.69f},{85.19f,-1.94f,1.77f},
 {165.93f,61.75f,1.79f},{51.08f,49.86f,1.79f},{107.10f,-26.39f,1.83f},{276.04f,-34.38f,1.85f},
 {206.89f,49.31f,1.86f},{89.88f,44.95f,1.90f},{99.43f,16.40f,1.93f},{37.95f,89.26f,1.98f},
 {141.90f,-8.66f,1.98f},{154.99f,19.84f,2.08f},{31.79f,23.46f,2.00f},{17.43f,35.62f,2.07f},
 {2.10f,29.09f,2.06f},{283.82f,-26.30f,2.05f},{211.67f,-36.37f,2.06f},{306.41f,-56.74f,1.94f}
};
#define W3_NSTARS ((int)(sizeof(W3_STARS)/sizeof(W3_STARS[0])))

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
