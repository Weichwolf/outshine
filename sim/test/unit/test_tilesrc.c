/* Unit tests for tilesrc.c — which tile sources exist and how they are addressed.
 *
 * The reason this file is worth its weight: Esri addresses imagery {z}/{y}/{x} while everyone
 * else uses {z}/{x}/{y}. Swapping two numbers there does not fail — it silently serves the
 * WRONG PLACE, which looks like "the map is a bit off" and survives for months. So the order
 * is pinned here explicitly, per source.
 */
#include "../../tiles/tilesrc.h"
#include "tassert.h"


int test_tilesrc(void) {
    printf("== tilesrc unit tests ==\n");
    char u[600];
    fb_tile_kind k;

    /* ---- names / parsing round-trip over every kind ---- */
    for (int i = 0; i < FB_TILE_KIND_COUNT; i++) {
        const char *n = fb_src_kind_name((fb_tile_kind)i);
        ck(n != NULL, "every kind has a name");
        fb_tile_kind back;
        ck(fb_src_kind_parse(n, &back) && back == (fb_tile_kind)i, "name parses back to its kind");
        ck(fb_src_content_type((fb_tile_kind)i) != NULL, "every kind has a content type");
        ck(fb_src_ext((fb_tile_kind)i) != NULL, "every kind has a cache extension");
        ck(fb_src_max_zoom((fb_tile_kind)i) > 0, "every kind has a max zoom");
    }
    ck_str(fb_src_kind_name(FB_TILE_TERRAIN), "terrain", "terrain name");
    ck_str(fb_src_kind_name(FB_TILE_VECTOR),  "vector",  "vector name");
    ck_str(fb_src_kind_name(FB_TILE_IMAGERY), "imagery", "imagery name");
    ck_str(fb_src_content_type(FB_TILE_VECTOR), "application/vnd.mapbox-vector-tile", "vector mime");
    ck_str(fb_src_content_type(FB_TILE_IMAGERY), "image/jpeg", "imagery mime");
    ck_str(fb_src_content_type(FB_TILE_TERRAIN), "image/png", "terrain mime");

    /* ---- invalid kinds must be rejected, not indexed out of bounds ---- */
    ck(fb_src_kind_name((fb_tile_kind)-1) == NULL, "negative kind -> NULL name");
    ck(fb_src_kind_name(FB_TILE_KIND_COUNT) == NULL, "out-of-range kind -> NULL name");
    ck(fb_src_content_type((fb_tile_kind)99) == NULL, "invalid kind -> NULL mime");
    ck(fb_src_ext((fb_tile_kind)99) == NULL, "invalid kind -> NULL ext");
    ck(fb_src_max_zoom((fb_tile_kind)99) == 0, "invalid kind -> zoom 0");
    ck(!fb_src_kind_parse("nope", &k), "unknown name is rejected");
    ck(!fb_src_kind_parse(NULL, &k), "NULL name is rejected");
    ck(!fb_src_kind_parse("terrain", NULL), "NULL out is rejected");

    /* ---- URL order: THE bug this file exists for ---- */
    ck(fb_src_url(FB_TILE_IMAGERY, 13, 4309, 2704, u, sizeof u), "imagery url builds");
    ck(strstr(u, "/tile/13/2704/4309") != NULL, "IMAGERY is {z}/{y}/{x} (y BEFORE x)");
    ck(strstr(u, "/tile/13/4309/2704") == NULL, "imagery must NOT be {z}/{x}/{y}");

    ck(fb_src_url(FB_TILE_TERRAIN, 13, 4309, 2704, u, sizeof u), "terrain url builds");
    ck(strstr(u, "/13/4309/2704.png") != NULL, "terrain is {z}/{x}/{y}");
    ck(strstr(u, "terrarium") != NULL, "terrain uses the terrarium encoding (not mapbox rgb)");

    ck(fb_src_url(FB_TILE_VECTOR, 13, 4309, 2704, u, sizeof u), "vector url builds");
    ck(strstr(u, "/13/4309/2704") != NULL, "vector is {z}/{x}/{y}");

    /* ---- range checks: never ask upstream for a tile that cannot exist ---- */
    ck(!fb_src_url((fb_tile_kind)99, 0, 0, 0, u, sizeof u), "invalid kind -> no url");
    ck(!fb_src_url(FB_TILE_VECTOR, -1, 0, 0, u, sizeof u), "negative zoom rejected");
    ck(!fb_src_url(FB_TILE_VECTOR, 15, 0, 0, u, sizeof u), "zoom above vector max (14) rejected");
    ck(fb_src_url(FB_TILE_VECTOR, 14, 0, 0, u, sizeof u), "zoom at vector max (14) accepted");
    ck(fb_src_url(FB_TILE_IMAGERY, 19, 0, 0, u, sizeof u), "imagery reaches z19");
    ck(!fb_src_url(FB_TILE_IMAGERY, 20, 0, 0, u, sizeof u), "zoom above imagery max rejected");
    ck(!fb_src_url(FB_TILE_TERRAIN, 16, 0, 0, u, sizeof u), "zoom above terrain max (15) rejected");
    /* the grid at zoom z is 2^z wide */
    ck(fb_src_url(FB_TILE_VECTOR, 1, 1, 1, u, sizeof u), "z1 x=1,y=1 is inside the grid");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, 2, 0, u, sizeof u), "z1 x=2 is off the grid");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, 0, 2, u, sizeof u), "z1 y=2 is off the grid");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, -1, 0, u, sizeof u), "negative x rejected");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, 0, -1, u, sizeof u), "negative y rejected");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, 0, 0, NULL, 10), "NULL buffer rejected");
    ck(!fb_src_url(FB_TILE_VECTOR, 1, 0, 0, u, 0), "zero-size buffer rejected");

    return 0;
}
