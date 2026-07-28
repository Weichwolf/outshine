/* Coordinate conversions: Web Mercator slippy tiles, MVT local coordinates, ENU tangent plane, WGS84
 * ECEF. Scope limits (ENU flat-earth range, Mercator latitude cap, MVT top-left origin):
 * doc/world/terrain.md, Abschnitt 5. */

#ifndef OSMMESH_GEO_H
#define OSMMESH_GEO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Geographic coordinate: WGS84 lon/lat in degrees, altitude in meters. */
typedef struct {
    double lon, lat, alt;
} osmmesh_geo;

/* Local tangent-plane ENU in meters, relative to a configured origin. */
typedef struct {
    double e, n, u;
} osmmesh_enu;

/* Geographic bounds (degrees). */
typedef struct {
    double min_lon, min_lat, max_lon, max_lat;
} osmmesh_geo_bounds;

/* Caches origin-dependent factors so per-vertex conversions are multiplies and adds. Opaque layout:
 * only osmmesh_enu_init may write these fields. */
typedef struct {
    double origin_lat, origin_lon;   /* input, degrees */
    double cos_lat0;                 /* cached cos(radians(origin_lat)) */
    double sin_lat0;                 /* cached sin(radians(origin_lat)) */
    double meters_per_deg_lat;       /* pi * R / 180 */
    double meters_per_deg_lon;       /* cos_lat0 * pi * R / 180 */
} osmmesh_enu_ctx;

/* WGS84 equatorial semi-major axis; extern so tests cross-check without re-hardcoding it. */
extern const double osmmesh_earth_radius_m;

/* Error codes. 0 = success, negative = error. */
#define OSMMESH_GEO_OK            0
#define OSMMESH_GEO_ERR_ARG      -1
#define OSMMESH_GEO_ERR_RANGE    -2   /* lat outside Mercator band */

/* The tile CONTAINING the point; exactly on a boundary the east/south tile wins (floor semantics). */
int osmmesh_geo_to_tile(double lon_deg, double lat_deg, uint8_t z,
                         uint32_t *out_x, uint32_t *out_y);

osmmesh_geo_bounds osmmesh_tile_bounds(uint8_t z, uint32_t x, uint32_t y);

/* MVT local coordinate -> geographic. Geometry just outside [0..extent] is accepted (features may
 * bleed) and projected by the same linear extrapolation. */
osmmesh_geo osmmesh_tile_local_to_geo(uint8_t z, uint32_t x, uint32_t y,
                                       uint32_t extent,
                                       int32_t local_x, int32_t local_y);

/* Fractional tile (fx,fy in [0..1], origin top-left) -> geographic in full double precision: the
 * canonical projector for the ECEF path, because the integer MVT lattice rounds ~0.18 m at z14 and
 * would break the sub-cm offset guarantee. The test suite pins it equal to the lattice variant. */
osmmesh_geo osmmesh_tile_frac_to_geo(uint8_t z, uint32_t x, uint32_t y,
                                      double fx, double fy);

/* ---- WGS84 ECEF: the global-scale path beside the ~20 km ENU model above ----
 * `alt`/`h` is height above the WGS84 ELLIPSOID; Terrarium heights are orthometric, and the geoid
 * undulation (~85 m globally) is ignored — a slow, smooth vertical bias with no visible effect. */

extern const double osmmesh_wgs84_a;   /* 6378137.0 m */
extern const double osmmesh_wgs84_f;   /* 1 / 298.257223563 */

/* X toward (lat 0, lon 0), Y toward (lat 0, lon 90E), Z toward the north pole. Right-handed. */
typedef struct {
    double x, y, z;
} osmmesh_ecef;

osmmesh_ecef osmmesh_geo_to_ecef(osmmesh_geo g);
/* Bowring's closed-form inverse, sub-mm over the whole ellipsoid. At the poles (x=y=0) lon is 0. */
osmmesh_geo osmmesh_ecef_to_geo(osmmesh_ecef p);

/* origin_lat must be in [-89.9, 89.9]: the ENU scale factor degenerates near the poles. */
int osmmesh_enu_init(osmmesh_enu_ctx *ctx,
                      double origin_lat, double origin_lon);

osmmesh_enu osmmesh_enu_from_geo(const osmmesh_enu_ctx *ctx,
                                   osmmesh_geo g);

osmmesh_geo osmmesh_enu_to_geo(const osmmesh_enu_ctx *ctx,
                                osmmesh_enu e);

/* Correct but NOT the hot path: use osmmesh_tile_enu_map_* for many vertices from the same tile. */
osmmesh_enu osmmesh_tile_local_to_enu(const osmmesh_enu_ctx *ctx,
                                        uint8_t z, uint32_t x, uint32_t y,
                                        uint32_t extent,
                                        int32_t local_x, int32_t local_y);

/* Per-vertex ENU fast path: (local_x, local_y) -> (e, n) is affine at the scale of one tile, and the
 * lat->n residual over a z14 tile (<1.5 km) is under a metre — swamped by the tangent-plane error. */
typedef struct {
    double origin_e, origin_n;   /* ENU position of tile top-left */
    double scale_e, scale_n;     /* meters per local-coord unit; scale_n NEGATIVE (local_y grows south) */
    uint32_t extent;             /* echoed from _init: tile width in meters = scale_e * extent */
} osmmesh_tile_enu_map;

void osmmesh_tile_enu_map_init(osmmesh_tile_enu_map *map,
                                const osmmesh_enu_ctx *ctx,
                                uint8_t z, uint32_t x, uint32_t y,
                                uint32_t extent);

/* Inline in the header so the compiler folds it into the mesh-builder inner loops. */
static inline osmmesh_enu osmmesh_tile_enu_map_apply(
    const osmmesh_tile_enu_map *map, int32_t local_x, int32_t local_y)
{
    osmmesh_enu r;
    r.e = map->origin_e + (double)local_x * map->scale_e;
    r.n = map->origin_n + (double)local_y * map->scale_n;
    r.u = 0.0;
    return r;
}

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_GEO_H */
