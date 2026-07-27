/* Coordinate library implementation; API in geo.h. Das ENU-Modell, seine Fehlerschranken und der
 * BEKANNTE ~0,3-m-Restfehler des tile_enu_map-Fast-Path (eine Eigenschaft, kein Bug — nicht durch
 * Trigonometrie in der heissen Schleife "reparieren"): doc/flightbox/world-and-terrain.md, Abschnitt 5.0. */

#include "geo.h"

#include <math.h>

const double osmmesh_earth_radius_m = 6378137.0;

/* atan(sinh(pi)) * 180/pi — the WMTS / OGC simple-tile-scheme bound. */
#define OSMMESH_MERCATOR_LAT_MAX  85.05112877980659

/* M_PI is not in C99 without extensions. */
static const double OSMMESH_PI = 3.14159265358979323846;
#define DEG2RAD(d)  ((d) * (OSMMESH_PI / 180.0))
#define RAD2DEG(r)  ((r) * (180.0 / OSMMESH_PI))

int osmmesh_geo_to_tile(double lon_deg, double lat_deg, uint8_t z,
                         uint32_t *out_x, uint32_t *out_y)
{
    if (!out_x || !out_y) return OSMMESH_GEO_ERR_ARG;
    if (lat_deg < -OSMMESH_MERCATOR_LAT_MAX ||
        lat_deg >  OSMMESH_MERCATOR_LAT_MAX) {
        return OSMMESH_GEO_ERR_RANGE;
    }

    double n = ldexp(1.0, (int)z);   /* 2^z exactly */
    double xf = (lon_deg + 180.0) / 360.0 * n;
    double yf = (1.0 - asinh(tan(DEG2RAD(lat_deg))) / OSMMESH_PI) * 0.5 * n;

    /* For z=0, n=1, so the only legal tile is 0. */
    double xc = floor(xf);
    double yc = floor(yf);
    if (xc < 0) xc = 0;
    if (yc < 0) yc = 0;
    if (xc > n - 1) xc = n - 1;
    if (yc > n - 1) yc = n - 1;

    *out_x = (uint32_t)xc;
    *out_y = (uint32_t)yc;
    return OSMMESH_GEO_OK;
}

osmmesh_geo_bounds osmmesh_tile_bounds(uint8_t z, uint32_t x, uint32_t y)
{
    osmmesh_geo_bounds b;
    double n = ldexp(1.0, (int)z);
    b.min_lon = (double)x       / n * 360.0 - 180.0;
    b.max_lon = (double)(x + 1) / n * 360.0 - 180.0;
    double yn = 1.0 - 2.0 * (double)y       / n;
    double ys = 1.0 - 2.0 * (double)(y + 1) / n;
    b.max_lat = RAD2DEG(atan(sinh(OSMMESH_PI * yn)));
    b.min_lat = RAD2DEG(atan(sinh(OSMMESH_PI * ys)));
    return b;
}

osmmesh_geo osmmesh_tile_local_to_geo(uint8_t z, uint32_t x, uint32_t y,
                                       uint32_t extent,
                                       int32_t local_x, int32_t local_y)
{
    osmmesh_geo g = { 0.0, 0.0, 0.0 };
    if (extent == 0) return g;   /* caller bug; avoid DIV0 */

    double n = ldexp(1.0, (int)z);
    double inv_extent = 1.0 / (double)extent;

    /* local_y grows south, matching the slippy-tile y convention directly, so this just adds. */
    double xf = (double)x + (double)local_x * inv_extent;
    double yf = (double)y + (double)local_y * inv_extent;

    g.lon = xf / n * 360.0 - 180.0;
    double yy = 1.0 - 2.0 * yf / n;
    g.lat = RAD2DEG(atan(sinh(OSMMESH_PI * yy)));
    g.alt = 0.0;
    return g;
}

int osmmesh_enu_init(osmmesh_enu_ctx *ctx,
                      double origin_lat, double origin_lon)
{
    if (!ctx) return OSMMESH_GEO_ERR_ARG;
    if (origin_lat < -89.9 || origin_lat > 89.9) return OSMMESH_GEO_ERR_RANGE;

    ctx->origin_lat = origin_lat;
    ctx->origin_lon = origin_lon;

    double lat_rad = DEG2RAD(origin_lat);
    ctx->cos_lat0 = cos(lat_rad);
    ctx->sin_lat0 = sin(lat_rad);

    double mpd = OSMMESH_PI * osmmesh_earth_radius_m / 180.0;  /* 111319.49... */
    ctx->meters_per_deg_lat = mpd;
    ctx->meters_per_deg_lon = ctx->cos_lat0 * mpd;
    return OSMMESH_GEO_OK;
}

osmmesh_enu osmmesh_enu_from_geo(const osmmesh_enu_ctx *ctx, osmmesh_geo g)
{
    osmmesh_enu r;
    r.e = (g.lon - ctx->origin_lon) * ctx->meters_per_deg_lon;
    r.n = (g.lat - ctx->origin_lat) * ctx->meters_per_deg_lat;
    r.u = g.alt;
    return r;
}

osmmesh_geo osmmesh_enu_to_geo(const osmmesh_enu_ctx *ctx, osmmesh_enu e)
{
    osmmesh_geo g;
    /* Exact for this model: both conversions are affine. */
    g.lon = ctx->origin_lon + e.e / ctx->meters_per_deg_lon;
    g.lat = ctx->origin_lat + e.n / ctx->meters_per_deg_lat;
    g.alt = e.u;
    return g;
}

osmmesh_enu osmmesh_tile_local_to_enu(const osmmesh_enu_ctx *ctx,
                                        uint8_t z, uint32_t x, uint32_t y,
                                        uint32_t extent,
                                        int32_t local_x, int32_t local_y)
{
    osmmesh_geo g = osmmesh_tile_local_to_geo(z, x, y, extent, local_x, local_y);
    return osmmesh_enu_from_geo(ctx, g);
}

/* Fast path: tile corners in ENU, interpolated linearly. */
void osmmesh_tile_enu_map_init(osmmesh_tile_enu_map *map,
                                const osmmesh_enu_ctx *ctx,
                                uint8_t z, uint32_t x, uint32_t y,
                                uint32_t extent)
{
    /* Two corners suffice: the affine map is defined by their ENU images. */
    osmmesh_geo_bounds b = osmmesh_tile_bounds(z, x, y);

    osmmesh_geo tl = { b.min_lon, b.max_lat, 0.0 };  /* top-left: W, N */
    osmmesh_geo br = { b.max_lon, b.min_lat, 0.0 };  /* bottom-right: E, S */

    osmmesh_enu etl = osmmesh_enu_from_geo(ctx, tl);
    osmmesh_enu ebr = osmmesh_enu_from_geo(ctx, br);

    map->origin_e = etl.e;
    map->origin_n = etl.n;

    double inv_extent = (extent == 0) ? 0.0 : 1.0 / (double)extent;
    map->scale_e = (ebr.e - etl.e) * inv_extent;
    map->scale_n = (ebr.n - etl.n) * inv_extent;
    map->extent  = extent;
}
