/* libosmmesh/include/osmmesh/geo.h
 *
 * Coordinate library: three layers of conversions used by all mesh builders.
 *
 *   1. Web Mercator (EPSG:3857 / Google-Slippy-Tiles) -- (z,x,y) <-> lon/lat.
 *   2. MVT local coordinates -- (0..extent) within a tile -> lon/lat.
 *   3. ENU (East/North/Up, meters) -- local tangent-plane around an origin.
 *
 * The library is purely computational. The only stateful piece is the ENU
 * context, which caches sin/cos of the configured origin so that hot per-
 * vertex conversions reduce to multiplies and adds.
 *
 * Scope limits (documented so T5+ callers don't re-learn them):
 *
 *   - ENU uses a flat-earth tangent-plane approximation, not a full ECEF
 *     round-trip. For our ~20 km ROI the error is sub-millimeter (see
 *     geo.c for the derivation). No ECEF, no WGS84 ellipsoidal formulas.
 *   - Web Mercator latitude is capped at +/- 85.05112878 deg. Points
 *     outside that band are rejected by osmmesh_geo_to_tile; callers
 *     should never need to project beyond that for slippy tiles.
 *   - MVT local_y origin is TOP-LEFT: local_y=0 is the northern edge,
 *     local_y=extent is the southern edge. This matches the Mapbox /
 *     Shortbread convention and contradicts the sign of the ENU n-axis,
 *     which is explicitly handled in the fast-path tile_enu_map.
 */

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

/* ENU context: caches origin-dependent factors. Initialize once per origin
 * with osmmesh_enu_init, reuse across any number of conversions. Treat as
 * opaque layout; only osmmesh_enu_init may write these fields. */
typedef struct {
    double origin_lat, origin_lon;   /* input, degrees */
    double cos_lat0;                 /* cached cos(radians(origin_lat)) */
    double sin_lat0;                 /* cached sin(radians(origin_lat)) */
    double meters_per_deg_lat;       /* pi * R / 180 */
    double meters_per_deg_lon;       /* cos_lat0 * pi * R / 180 */
} osmmesh_enu_ctx;

/* Earth-radius constant used for the tangent-plane scaling. WGS84 equatorial
 * semi-major axis. Exposed as extern so tests can cross-check scale factors
 * without re-hardcoding the value. */
extern const double osmmesh_earth_radius_m;

/* Error codes. 0 = success, negative = error. */
#define OSMMESH_GEO_OK            0
#define OSMMESH_GEO_ERR_ARG      -1
#define OSMMESH_GEO_ERR_RANGE    -2   /* lat outside Mercator band */

/* ========================================================================
 *  Web Mercator / slippy-tile math
 * ====================================================================== */

/* (lon_deg, lat_deg) -> tile (z, x, y). The integer tile that CONTAINS the
 * point is written to *out_x / *out_y. If the point lies exactly on a tile
 * boundary, the tile to the east / south wins (standard floor semantics on
 * the continuous tile coordinate).
 *
 * Returns OSMMESH_GEO_OK on success; OSMMESH_GEO_ERR_ARG on NULL out pointers;
 * OSMMESH_GEO_ERR_RANGE if lat_deg is outside [-85.05112878, 85.05112878]. */
int osmmesh_geo_to_tile(double lon_deg, double lat_deg, uint8_t z,
                         uint32_t *out_x, uint32_t *out_y);

/* Geographic bounds of a tile. Never fails for in-range inputs; for z=0 the
 * entire Mercator world is returned. */
osmmesh_geo_bounds osmmesh_tile_bounds(uint8_t z, uint32_t x, uint32_t y);

/* Convert an MVT local coordinate (inside tile (z,x,y)) to geographic
 * (lon, lat, alt=0). local_x, local_y are in [0..extent]; the origin is the
 * tile's TOP-LEFT (local_y=0 is the northern edge). Geometry just outside
 * [0..extent] is accepted (MVT features may bleed) and projected via the
 * same linear extrapolation. */
osmmesh_geo osmmesh_tile_local_to_geo(uint8_t z, uint32_t x, uint32_t y,
                                       uint32_t extent,
                                       int32_t local_x, int32_t local_y);

/* ========================================================================
 *  ENU (local tangent plane)
 * ====================================================================== */

/* Initialize ctx with a geographic origin. origin_lat must be in
 * [-89.9, 89.9] degrees (the ENU scale factor degenerates near the poles).
 * Returns OSMMESH_GEO_OK or OSMMESH_GEO_ERR_ARG / OSMMESH_GEO_ERR_RANGE. */
int osmmesh_enu_init(osmmesh_enu_ctx *ctx,
                      double origin_lat, double origin_lon);

/* Geographic -> local ENU meters. Tangent-plane approximation anchored at
 * the context origin: sub-millimeter error at 20 km range (see geo.c). */
osmmesh_enu osmmesh_enu_from_geo(const osmmesh_enu_ctx *ctx,
                                   osmmesh_geo g);

/* Inverse: local ENU -> geographic. */
osmmesh_geo osmmesh_enu_to_geo(const osmmesh_enu_ctx *ctx,
                                osmmesh_enu e);

/* Convenience path: MVT local coord -> ENU meters. Correct but NOT on the
 * hot path: use osmmesh_tile_enu_map_* if you are doing many vertices from
 * the same tile. */
osmmesh_enu osmmesh_tile_local_to_enu(const osmmesh_enu_ctx *ctx,
                                        uint8_t z, uint32_t x, uint32_t y,
                                        uint32_t extent,
                                        int32_t local_x, int32_t local_y);

/* ------------------------------------------------------------------------
 * Fast path for per-vertex ENU projection within one tile.
 *
 * The mesh builders (T5/T6/T7) iterate over thousands of MVT vertices per
 * tile. Evaluating the lon/lat trig per vertex is wasteful: the relation
 * between (local_x, local_y) and (e, n) is affine at the scale of one
 * tile (the Mercator -> lon stretch is linear on a tile; the lon -> e
 * stretch is linear in the tangent plane; the lat->n stretch is very
 * nearly linear over the vertical extent of one z=14 tile (<1.5 km) and
 * we treat it as linear; the residual is < 1 m and swamped by the
 * tangent-plane model error anyway).
 *
 * Usage pattern:
 *     osmmesh_tile_enu_map map;
 *     osmmesh_tile_enu_map_init(&map, &ctx, z, x, y, extent);
 *     for (each vertex) {
 *         osmmesh_enu v = osmmesh_tile_enu_map_apply(&map, lx, ly);
 *         ...
 *     }
 * ------------------------------------------------------------------------ */
typedef struct {
    double origin_e, origin_n;   /* ENU position of tile top-left */
    double scale_e, scale_n;     /* meters per local-coord unit */
    /* scale_n is NEGATIVE because MVT local_y grows south but ENU n-axis
     * grows north. */
    uint32_t extent;             /* local-coord span of the tile (echoed
                                  * from _init for downstream callers that
                                  * need to know the tile width in meters
                                  * = scale_e * extent). Added in T5. */
} osmmesh_tile_enu_map;

void osmmesh_tile_enu_map_init(osmmesh_tile_enu_map *map,
                                const osmmesh_enu_ctx *ctx,
                                uint8_t z, uint32_t x, uint32_t y,
                                uint32_t extent);

/* Per-vertex fast path: only multiplies and adds, no trig, no branches.
 * Kept static inline in the header so the compiler can fold it into the
 * mesh-builder inner loops. */
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
