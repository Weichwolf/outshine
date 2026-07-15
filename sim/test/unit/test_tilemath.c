/* Unit tests for tilemath.h — the pure geo<->tile core.
 *
 * This file exists because a missing cos(lat) in an ad-hoc conversion once put the rendered
 * world 1.6x too far east and failed 1032 physics-suite cases. Every conversion now lives in
 * one header, and this pins it down with known-correct anchors and round-trips.
 *
 * Target: 100% line coverage of tilemath.h. It is pure maths — there is no excuse for less.
 */
#include "../../tiles/tilemath.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_run = 0;

static void ck(int cond, const char *what) {
    g_run++;
    if (!cond) { g_fail++; printf("  [FAIL] %s\n", what); }
}
static void ck_near(double got, double want, double tol, const char *what) {
    g_run++;
    double d = got - want; if (d < 0) d = -d;
    if (d > tol) { g_fail++; printf("  [FAIL] %s: got %.6f want %.6f (tol %.g)\n", what, got, want, tol); }
}

int main(void) {
    printf("== tilemath unit tests ==\n");

    /* ---- fb_clampd ---- */
    ck_near(fb_clampd(5, 0, 10), 5, 1e-12, "clamp inside");
    ck_near(fb_clampd(-1, 0, 10), 0, 1e-12, "clamp low");
    ck_near(fb_clampd(11, 0, 10), 10, 1e-12, "clamp high");

    /* ---- fb_geo_to_tile: known anchors ----
     * z0 holds the whole world in one tile, so null island sits at its exact centre. */
    double tx, ty;
    fb_geo_to_tile(0, 0, 0, &tx, &ty);
    ck_near(tx, 0.5, 1e-9, "z0 lon=0 -> tx=0.5");
    ck_near(ty, 0.5, 1e-9, "z0 lat=0 -> ty=0.5");

    /* the antimeridian is the left and right edge of the map */
    fb_geo_to_tile(0, -180, 1, &tx, &ty);
    ck_near(tx, 0.0, 1e-9, "z1 lon=-180 -> tx=0");
    fb_geo_to_tile(0, 180, 1, &tx, &ty);
    ck_near(tx, 2.0, 1e-9, "z1 lon=+180 -> tx=2 (right edge)");

    /* north is ty=0, south is ty=n */
    fb_geo_to_tile(FB_MERC_LAT_MAX, 0, 1, &tx, &ty);
    ck_near(ty, 0.0, 1e-6, "z1 lat=+85.05 -> ty=0 (north edge)");
    fb_geo_to_tile(-FB_MERC_LAT_MAX, 0, 1, &tx, &ty);
    ck_near(ty, 2.0, 1e-6, "z1 lat=-85.05 -> ty=2 (south edge)");

    /* beyond the Mercator limit must CLAMP, not produce inf/NaN */
    fb_geo_to_tile(89.9, 0, 1, &tx, &ty);
    ck(ty >= 0.0 && ty <= 2.0, "lat beyond mercator limit clamps (north)");
    fb_geo_to_tile(-89.9, 400, 1, &tx, &ty);
    ck(ty >= 0.0 && ty <= 2.0 && tx >= 0.0 && tx <= 2.0, "lat/lon beyond limits clamp (south/east)");

    /* ---- round-trip: the inverse must recover the input ---- */
    struct { double lat, lon; int z; const char *name; } pts[] = {
        { 52.045,   9.385, 13, "Hameln (home)" },
        { 47.283,   7.524, 13, "Grenchen (CH)" },
        { 45.833,   6.865, 14, "Mont Blanc" },
        {-33.868, 151.209, 12, "Sydney (S/E)" },
        { 64.146, -21.942, 10, "Reykjavik (N/W)" },
        {  0.000,   0.000,  8, "null island" },
    };
    for (size_t i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        double a, b, la, lo;
        fb_geo_to_tile(pts[i].lat, pts[i].lon, pts[i].z, &a, &b);
        fb_tile_to_geo(a, b, pts[i].z, &la, &lo);
        char m[96];
        snprintf(m, sizeof m, "round-trip lat %s", pts[i].name); ck_near(la, pts[i].lat, 1e-7, m);
        snprintf(m, sizeof m, "round-trip lon %s", pts[i].name); ck_near(lo, pts[i].lon, 1e-7, m);
    }

    /* ---- fb_tile_res_m: the textbook value at the equator ---- */
    ck_near(fb_tile_res_m(0, 0), 156543.0339, 1e-3, "res at equator z0 = 156543 m/px");
    ck_near(fb_tile_res_m(0, 1), 78271.5169, 1e-3, "res halves each zoom");
    /* z13 over home is the DEM zoom the elevation service uses — pin the claim "~12 m/px" */
    ck_near(fb_tile_res_m(52.045, 13), 11.746, 1e-2, "res at home z13 ~ 11.75 m/px");
    /* cos(lat) shrinks ground resolution away from the equator — the factor that was once missing */
    ck(fb_tile_res_m(52.045, 13) < fb_tile_res_m(0, 13), "res shrinks with latitude (cos factor present)");

    /* ---- fb_tile_wrap ---- */
    long x, y;
    x = -1; y = 3; ck(fb_tile_wrap(3, &x, &y) && x == 7, "x wraps negative (antimeridian)");
    x = 8;  y = 3; ck(fb_tile_wrap(3, &x, &y) && x == 0, "x wraps past n");
    x = 3;  y = 3; ck(fb_tile_wrap(3, &x, &y) && x == 3 && y == 3, "in-range passes through");
    x = 0;  y = -1; ck(!fb_tile_wrap(3, &x, &y), "y above north pole is rejected");
    x = 0;  y = 8;  ck(!fb_tile_wrap(3, &x, &y), "y below south pole is rejected");

    /* ---- fb_bilinear ---- */
    float g[4] = { 0, 10,
                   20, 30 };                       /* 2x2: rows-major */
    ck_near(fb_bilinear(g, 2, 2, 0, 0), 0, 1e-6, "bilinear corner (0,0)");
    ck_near(fb_bilinear(g, 2, 2, 1, 0), 10, 1e-6, "bilinear corner (1,0)");
    ck_near(fb_bilinear(g, 2, 2, 0, 1), 20, 1e-6, "bilinear corner (0,1)");
    ck_near(fb_bilinear(g, 2, 2, 1, 1), 30, 1e-6, "bilinear corner (1,1)");
    ck_near(fb_bilinear(g, 2, 2, 0.5, 0.5), 15, 1e-6, "bilinear centre = mean");
    ck_near(fb_bilinear(g, 2, 2, 0.5, 0), 5, 1e-6, "bilinear along top edge");
    ck_near(fb_bilinear(g, 2, 2, 0, 0.5), 10, 1e-6, "bilinear along left edge");
    /* out-of-range must clamp to the edge, never read out of bounds */
    ck_near(fb_bilinear(g, 2, 2, -5, -5), 0, 1e-6, "bilinear clamps below range");
    ck_near(fb_bilinear(g, 2, 2, 99, 99), 30, 1e-6, "bilinear clamps above range");
    /* degenerate inputs must be safe, not crash */
    ck_near(fb_bilinear(NULL, 2, 2, 0, 0), 0, 1e-6, "bilinear null grid -> 0");
    ck_near(fb_bilinear(g, 0, 2, 0, 0), 0, 1e-6, "bilinear zero cols -> 0");
    ck_near(fb_bilinear(g, 2, 0, 0, 0), 0, 1e-6, "bilinear zero rows -> 0");
    /* a 1-wide grid must not step past its only column */
    float one[1] = { 42 };
    ck_near(fb_bilinear(one, 1, 1, 0.7, 0.7), 42, 1e-6, "bilinear 1x1 grid");

    printf("== %d/%d assertions passed ==\n", g_run - g_fail, g_run);
    return g_fail ? 1 : 0;
}
