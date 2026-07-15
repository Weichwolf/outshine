/* Unit tests for route.c — parsing tile requests and query parameters.
 *
 * A route parser that mis-reads a coordinate does not error: it serves a plausible-looking
 * WRONG tile. And a query parser that matches "lat" inside "sublat" silently reads the wrong
 * number. Both are pinned here.
 */
#include "../../tiles/route.h"
#include "tassert.h"
#include <math.h>


int test_route(void) {
    printf("== route unit tests ==\n");
    fb_tile_kind k; int z; long x, y; double d;

    /* ---- happy paths, every kind ---- */
    ck(fb_route_tile("/t/terrain/13/4309/2704", &k, &z, &x, &y) &&
       k == FB_TILE_TERRAIN && z == 13 && x == 4309 && y == 2704, "parse terrain route");
    ck(fb_route_tile("/t/vector/14/8619/5408", &k, &z, &x, &y) &&
       k == FB_TILE_VECTOR && z == 14 && x == 8619 && y == 5408, "parse vector route");
    ck(fb_route_tile("/t/imagery/19/1/2", &k, &z, &x, &y) &&
       k == FB_TILE_IMAGERY && z == 19 && x == 1 && y == 2, "parse imagery route");
    ck(fb_route_tile("/t/terrain/0/0/0", &k, &z, &x, &y) && z == 0 && x == 0 && y == 0,
       "z0 origin tile");
    /* a file extension is conventional and must be tolerated */
    ck(fb_route_tile("/t/terrain/13/4309/2704.png", &k, &z, &x, &y) && x == 4309, "trailing .png ok");
    ck(fb_route_tile("/t/vector/13/4309/2704.pbf", &k, &z, &x, &y) && y == 2704, "trailing .pbf ok");

    /* ---- rejects ---- */
    ck(!fb_route_tile("/elev", &k, &z, &x, &y), "non-tile path rejected");
    ck(!fb_route_tile("/t/", &k, &z, &x, &y), "bare /t/ rejected");
    ck(!fb_route_tile("/t//13/1/1", &k, &z, &x, &y), "empty kind rejected");
    ck(!fb_route_tile("/t/bogus/13/1/1", &k, &z, &x, &y), "unknown kind rejected");
    ck(!fb_route_tile("/t/terrain/13/1", &k, &z, &x, &y), "missing y rejected");
    ck(!fb_route_tile("/t/terrain/13", &k, &z, &x, &y), "missing x and y rejected");
    ck(!fb_route_tile("/t/terrain/a/1/1", &k, &z, &x, &y), "non-numeric zoom rejected");
    ck(!fb_route_tile("/t/terrain/13/x/1", &k, &z, &x, &y), "non-numeric x rejected");
    ck(!fb_route_tile("/t/terrain/13/1/y", &k, &z, &x, &y), "non-numeric y rejected");
    ck(!fb_route_tile("/t/terrain/13/1/1/extra", &k, &z, &x, &y), "trailing garbage rejected");
    ck(!fb_route_tile("/t/terrain/-1/1/1", &k, &z, &x, &y), "negative zoom rejected (no minus sign)");
    ck(!fb_route_tile("/t/vector/15/1/1", &k, &z, &x, &y), "zoom above the source max rejected");
    ck(!fb_route_tile("/t/vector/1/2/0", &k, &z, &x, &y), "x off the 2^z grid rejected");
    ck(!fb_route_tile("/t/vector/1/0/2", &k, &z, &x, &y), "y off the 2^z grid rejected");
    ck(!fb_route_tile("/t/terrain/999999999999/1/1", &k, &z, &x, &y), "absurd number rejected, not overflowed");
    ck(!fb_route_tile("/t/waaaaaaaaaaaaaaaaaaaaaaaaaaaaay-too-long/1/1/1", &k, &z, &x, &y),
       "over-long kind rejected (no buffer overrun)");
    ck(!fb_route_tile(NULL, &k, &z, &x, &y), "NULL path rejected");
    ck(!fb_route_tile("/t/terrain/1/0/0", NULL, &z, &x, &y), "NULL out rejected");

    /* ---- query parameters ---- */
    ck(fb_query_double("lat=52.045&lon=9.385", "lat", &d) && fabs(d - 52.045) < 1e-9, "read lat");
    ck(fb_query_double("lat=52.045&lon=9.385", "lon", &d) && fabs(d - 9.385) < 1e-9, "read lon (2nd param)");
    ck(fb_query_double("lon=-9.5", "lon", &d) && fabs(d + 9.5) < 1e-9, "negative value");
    ck(fb_query_double("a=1&b=2&lat=3", "lat", &d) && fabs(d - 3) < 1e-9, "read last param");
    /* whole-key matching: "lat" must not match inside "sublat" */
    ck(!fb_query_double("sublat=1", "lat", &d), "key must match whole, not as a suffix");
    ck(fb_query_double("sublat=1&lat=2", "lat", &d) && fabs(d - 2) < 1e-9, "picks the real key, not the suffix");
    ck(!fb_query_double("lat=", "lat", &d), "empty value rejected");
    ck(!fb_query_double("lat=abc", "lat", &d), "non-numeric value rejected");
    ck(!fb_query_double("lon=1", "lat", &d), "absent key rejected");
    ck(!fb_query_double(NULL, "lat", &d), "NULL query rejected");
    ck(!fb_query_double("lat=1", NULL, &d), "NULL key rejected");
    ck(!fb_query_double("lat=1", "lat", NULL), "NULL out rejected");
    ck(!fb_query_double("", "lat", &d), "empty query rejected");

    return 0;
}
