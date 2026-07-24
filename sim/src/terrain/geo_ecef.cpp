/* libosmmesh/src/geo_ecef.c
 *
 * WGS84 ECEF conversions + a full-precision fractional-tile projector.
 *
 * ADDITIVE global-scale path. Kept in its own translation unit (not folded
 * into geo.c) for one concrete reason: geo.c's osmmesh_geo_to_tile carries a
 * defensive `yc < 0` clamp that its own latitude range-guard makes
 * unreachable, so geo.c cannot be pulled into the 100%-coverage unit suite
 * without either deleting a guard whose risk this task does not own, or gaming
 * the number. This file has no such dead branch and is verified to 100%.
 *
 * The ENU model in geo.c is left exactly as-is; the renderer still consumes it.
 *
 * See osmmesh/geo.h for the API contract and the altitude convention.
 */

#include "geo.h"

#include <math.h>

/* Deg <-> rad without M_PI (not in C99 without extensions). Same value as
 * geo.c's private copy. */
static const double OSMMESH_PI = 3.14159265358979323846;
#define DEG2RAD(d)  ((d) * (OSMMESH_PI / 180.0))
#define RAD2DEG(r)  ((r) * (180.0 / OSMMESH_PI))

/* ========================================================================
 *  WGS84 constants
 * ====================================================================== */

const double osmmesh_wgs84_a = 6378137.0;
const double osmmesh_wgs84_f = 1.0 / 298.257223563;

/* ========================================================================
 *  Fractional-tile -> geographic (full double precision)
 * ====================================================================== */

osmmesh_geo osmmesh_tile_frac_to_geo(uint8_t z, uint32_t x, uint32_t y,
                                      double fx, double fy)
{
    double n = ldexp(1.0, (int)z);       /* 2^z exactly */
    double xf = ((double)x + fx) / n;
    double yf = ((double)y + fy) / n;

    osmmesh_geo g;
    g.lon = xf * 360.0 - 180.0;
    /* Web-Mercator inverse: identical to geo.c osmmesh_tile_local_to_geo. The
     * test suite pins the two together at integer local coords. */
    double yy = 1.0 - 2.0 * yf;
    g.lat = RAD2DEG(atan(sinh(OSMMESH_PI * yy)));
    g.alt = 0.0;
    return g;
}

/* ========================================================================
 *  Geodetic <-> ECEF (WGS84)
 * ====================================================================== */

osmmesh_ecef osmmesh_geo_to_ecef(osmmesh_geo g)
{
    double a  = osmmesh_wgs84_a;
    double f  = osmmesh_wgs84_f;
    double e2 = f * (2.0 - f);           /* first eccentricity squared */

    double phi = DEG2RAD(g.lat);
    double lam = DEG2RAD(g.lon);
    double sphi = sin(phi), cphi = cos(phi);
    double slam = sin(lam), clam = cos(lam);

    double N = a / sqrt(1.0 - e2 * sphi * sphi);

    osmmesh_ecef p;
    p.x = (N + g.alt) * cphi * clam;
    p.y = (N + g.alt) * cphi * slam;
    p.z = (N * (1.0 - e2) + g.alt) * sphi;
    return p;
}

osmmesh_geo osmmesh_ecef_to_geo(osmmesh_ecef p)
{
    double a  = osmmesh_wgs84_a;
    double f  = osmmesh_wgs84_f;
    double e2 = f * (2.0 - f);
    double b  = a * (1.0 - f);           /* semi-minor axis */

    double pxy = sqrt(p.x * p.x + p.y * p.y);   /* distance from spin axis */

    osmmesh_geo g;

    /* Polar singularity: on the spin axis longitude is undefined; define 0. */
    if (pxy < 1e-9) {
        g.lon = 0.0;
        g.lat = (p.z >= 0.0) ? 90.0 : -90.0;
        g.alt = fabs(p.z) - b;
        return g;
    }

    g.lon = RAD2DEG(atan2(p.y, p.x));

    /* Bowring 1976: closed-form geodetic latitude via the parametric latitude
     * theta. ep2 = second eccentricity squared. */
    double ep2 = e2 / (1.0 - e2);
    double theta = atan2(p.z * a, pxy * b);
    double st = sin(theta), ct = cos(theta);
    double lat = atan2(p.z + ep2 * b * st * st * st,
                       pxy - e2 * a * ct * ct * ct);
    double slat = sin(lat), clat = cos(lat);
    double N = a / sqrt(1.0 - e2 * slat * slat);

    g.lat = RAD2DEG(lat);
    g.alt = pxy / clat - N;
    return g;
}
