/* libosmmesh/src/building_internal.h
 *
 * Private shared types for the T6 building generator. Not part of the public
 * API — do NOT move to include/osmmesh/.
 */

#ifndef OSMMESH_BUILDING_INTERNAL_H
#define OSMMESH_BUILDING_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "osmmesh/building.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * Footprint
 * ------------------------------------------------------------------------
 *
 * Outer-ring polygon in ENU meters, 2-D (e, n). Stored AoS as [e0, n0, e1,
 * n1, ...] for ear-clipping friendliness. Ring is CLOSED-as-CW in the ENU
 * basis (see building.h winding note). The last vertex != the first (MVT
 * ClosePath is stripped by the decoder).
 */
#define OSMMESH_BUILDING_MAX_VERTS 512

typedef struct {
    float    xy[OSMMESH_BUILDING_MAX_VERTS * 2];
    uint32_t n;
    float    signed_area;    /* CCW-positive convention; CW rings yield <0. */
    float    area_abs;
    /* OBB. `obb_long`/`obb_short` in meters; `obb_axis[0..1]` is the unit
     * vector along the long axis. Corners are CCW around the OBB. */
    float    obb_corners[4][2];
    float    obb_long, obb_short;
    float    obb_axis[2];
    float    obb_center[2];
    /* Hull ratio := area_abs / convex_hull_area. 1.0 for a convex poly;
     * < 1 for concave footprints. */
    float    hull_ratio;
    int      is_rectangular;
    int      is_regular;
} osmmesh_building_footprint;

/* ------------------------------------------------------------------------
 * RNG — xorshift64*, seeded by osm_id (or a coord-stream hash if osm_id=0).
 * ------------------------------------------------------------------------ */

typedef struct { uint64_t state; } osmmesh_building_rng;

void    osmmesh_building_rng_seed(osmmesh_building_rng *r, uint64_t seed);
uint64_t osmmesh_building_rng_u64 (osmmesh_building_rng *r);
/* Uniform in [0, 1). */
float    osmmesh_building_rng_f01 (osmmesh_building_rng *r);
/* Uniform integer in [0, n). n > 0. */
uint32_t osmmesh_building_rng_u32 (osmmesh_building_rng *r, uint32_t n);
/* Hash used as a fallback seed when osm_id == 0: FNV-1a over the raw MVT
 * coord stream. Never returns 0 (folded to 1) so downstream xorshift has a
 * non-degenerate state. */
uint64_t osmmesh_building_rng_hash_u32(const uint32_t *data, size_t n);

/* ------------------------------------------------------------------------
 * Footprint analysis (building_footprint.c)
 * ------------------------------------------------------------------------ */

/* Project the feature's first (outer) ring through `map` into ENU meters,
 * strip a trailing duplicate, and analyze it (OBB, hull ratio, flags).
 * Returns 0 on success; negative OSMMESH_BUILDING_ERR_* on degenerate input. */
int osmmesh_building_footprint_build(const osmmesh_mvt_feature  *feature,
                                      const osmmesh_tile_enu_map *map,
                                      osmmesh_building_footprint *out);

/* Signed area (CCW-positive) of an arbitrary 2-D polygon. */
float osmmesh_building_polygon_signed_area(const float *xy, uint32_t n);

/* Andrew's monotone chain. `out_indices` must have capacity >= n.
 * Returns the hull vertex count (>= 3 on well-formed input; <3 on collinear). */
uint32_t osmmesh_building_convex_hull(const float *xy, uint32_t n,
                                       uint32_t *out_indices);

/* Rotating-calipers OBB over the convex hull. Writes `obb_*` fields. */
void osmmesh_building_obb(const float *xy, uint32_t n,
                           float out_corners[4][2],
                           float *out_long, float *out_short,
                           float out_axis[2], float out_center[2]);

/* ------------------------------------------------------------------------
 * Terrain-aware build path (T12)
 *
 * Internal entry point used by osmmesh.c when a terrain heightgrid is
 * available. Otherwise behaves exactly like osmmesh_building_build:
 *
 *   - If `terrain_grid` is NULL, all Z coordinates are 0 (MVP behavior).
 *   - Else the building's base is `max(sample(xy_i))` over the footprint
 *     vertices (plus a fixed clearance), the wall top is `base + wall_h`
 *     (horizontal ridge), and each wall quad's bottom Z is re-sampled per
 *     vertex so the wall skirt follows the hillside.
 *
 * See building.c::build_walls for the wall-drape math. The public entry
 * osmmesh_building_build forwards here with terrain_grid=NULL.
 * ------------------------------------------------------------------------ */

/* osmmesh_terrain_grid is a typedef'd struct in terrain.h — include it
 * here rather than forward-declaring (forward decls of typedef'd types
 * don't unify with the typedef under -Wincompatible-pointer-types). */
#include "osmmesh/terrain.h"

int osmmesh_building_build_with_terrain(
    const osmmesh_mvt_layer     *layer,
    const osmmesh_mvt_feature   *feature,
    const osmmesh_tile_enu_map  *map,
    const osmmesh_building_opts *opts,
    const osmmesh_terrain_grid  *terrain_grid,   /* may be NULL */
    osmmesh_mesh                *out_mesh,
    osmmesh_building_info       *out_info);

/* ------------------------------------------------------------------------
 * Ear-clipping triangulation (building_triangulate.c)
 * ------------------------------------------------------------------------
 *
 * Input is a simple polygon (no holes) with vertices in `xy` (n*2 floats).
 * `ccw` selects the expected orientation — the algorithm only cuts ears on
 * that side. `out_tris` must hold at least 3*(n-2) uint32_t.
 *
 * Returns the number of triangles written (n-2 on success, 0 on failure).
 */
uint32_t osmmesh_building_ear_clip(const float *xy, uint32_t n,
                                    int ccw, uint32_t *out_tris);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_BUILDING_INTERNAL_H */
