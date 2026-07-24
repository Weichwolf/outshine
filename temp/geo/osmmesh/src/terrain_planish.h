/* libosmmesh/src/terrain_planish.h
 *
 * Internal helpers for T12 terrain-aware integration:
 *
 *   1. Bilinear sampling a heightfield at an arbitrary ENU coordinate.
 *   2. "Planishing" — rasterising a set of classified polylines into a
 *      heightmap so that each polyline corridor is replaced by a moving-
 *      median smoothed profile, with a cosine-weighted edge falloff into
 *      the surrounding terrain. Overlaps are arbitrated by a class-priority
 *      table declared in osmmesh/linear.h.
 *
 * Pure internal header — not installed under include/osmmesh/.
 *
 * All API here borrows the heightgrid; no ownership transfer. The heights
 * buffer must be writable for osmmesh_planish_rasterise_linears.
 */

#ifndef OSMMESH_TERRAIN_PLANISH_H
#define OSMMESH_TERRAIN_PLANISH_H

#include <stdint.h>

#include "osmmesh/geo.h"
#include "osmmesh/linear.h"
#include "osmmesh/mvt.h"
#include "osmmesh/terrain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 *  Bilinear sample
 *
 *  Sample `grid` at ENU coordinate (e, n). The grid origin (row 0, col 0)
 *  sits at `map->origin_e, map->origin_n` (tile top-left), and one grid
 *  step in the horizontal direction spans `scale_e * extent / (cols-1)` m
 *  east, one step in the vertical direction spans `scale_n * extent /
 *  (rows-1)` m north (scale_n < 0).
 *
 *  Points outside the grid clamp to the nearest edge pixel — callers with
 *  coordinates that straddle the tile boundary will see a flat patch in
 *  the skirt region. Acceptable for MVP; a later phase can reach into
 *  neighbour tiles.
 * ------------------------------------------------------------------------ */
float osmmesh_planish_sample_bilinear(const osmmesh_terrain_grid *grid,
                                       const osmmesh_tile_enu_map *map,
                                       float e, float n);

/* ------------------------------------------------------------------------
 *  Collect one linear feature's polylines into an ENU-projected flat buffer
 *  that the rasteriser can consume in one shot. The buffer has one entry
 *  per sub-string; the caller owns the memory.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* AoS [e0, n0, e1, n1, ...] of this sub-string's spine in ENU meters. */
    float              *xy;
    uint32_t            n;           /* number of spine vertices (>= 2) */
    float               width_m;     /* full ribbon width */
    osmmesh_linear_kind kind;
    int                 priority;    /* osmmesh_linear_class_priority(kind) */
} osmmesh_planish_line;

typedef struct {
    osmmesh_planish_line *items;
    uint32_t              count;
    uint32_t              cap;
} osmmesh_planish_line_set;

void osmmesh_planish_line_set_init (osmmesh_planish_line_set *s);
void osmmesh_planish_line_set_free (osmmesh_planish_line_set *s);
int  osmmesh_planish_line_set_append(osmmesh_planish_line_set *s,
                                       const osmmesh_planish_line *line);

/* Collect all linestrings from one MVT layer into `out`, using the same
 * classification / width / skip rules as osmmesh_linear_build but without
 * emitting any mesh. Returns OSMMESH_LINEAR_OK, OSMMESH_LINEAR_ERR_OOM, or
 * OSMMESH_LINEAR_ERR_ARG. Per-feature skips are silently discarded. */
int  osmmesh_planish_collect_layer(const osmmesh_mvt_layer    *layer,
                                    const osmmesh_tile_enu_map *map,
                                    const osmmesh_linear_opts  *opts,
                                    osmmesh_planish_line_set   *out);

/* ------------------------------------------------------------------------
 *  Rasterise a collected line-set into `grid`. The grid is the "flat"
 *  copy — it is written in-place. Class priority is honoured: features
 *  are processed lowest-priority-first so that higher classes overwrite
 *  lower ones on overlap.
 *
 *  Each pixel records which priority it last accepted (via a caller-owned
 *  uint8_t parallel buffer of length rows*cols). Subsequent writes with a
 *  STRICTLY lower priority are dropped. Pass NULL for the scratch to let
 *  this function allocate + free a local one.
 *
 *  Returns 0 on success, -1 on OOM.
 * ------------------------------------------------------------------------ */
int osmmesh_planish_rasterise_linears(osmmesh_terrain_grid       *grid,
                                       const osmmesh_tile_enu_map *map,
                                       osmmesh_planish_line_set   *lines);

/* Subtractive variant: every pixel under any ribbon (core + cosine
 * falloff, same geometry as rasterise_linears) has `depth_m` subtracted
 * from its height. Overlapping features don't stack — max(amount) per
 * pixel. Terrain keeps its natural gradient; the road just sinks into
 * a shallow cut. Cross-tile stable when every tile sees the same
 * buffer-zone features (i.e. the caller did NOT apply a centroid-skip
 * filter before collecting lines). Returns 0 / -1 on OOM.
 *
 * NOTE: superseded by osmmesh_terrain_notch_under_linear_meshes (T14.8)
 * for production use; kept around for the planish-priority unit tests. */
int osmmesh_planish_notch_linears(osmmesh_terrain_grid       *grid,
                                   const osmmesh_tile_enu_map *map,
                                   const osmmesh_planish_line_set *lines,
                                   float                       depth_m);

/* Triangle-direct notch: walks every triangle of every linear mesh,
 * sweeps the heightgrid pixels inside the triangle's xy bounding box,
 * and for each pixel whose xy lies inside the triangle (in 2-D) sets
 * the pixel height to min(current, z_linear_at_pixel - margin_m). This
 * makes the "every linear triangle is above the terrain mesh" invariant
 * hold by construction (subject to `margin_m` slack), regardless of
 * polyline density or class-bridge limits. Linear meshes must already
 * carry their final draped z. Cross-tile stable when neighbour tiles
 * see the same MVT buffer-zone features. */
int osmmesh_terrain_notch_under_linear_meshes(
        osmmesh_terrain_grid       *grid,
        const osmmesh_tile_enu_map *map,
        const osmmesh_mesh         *meshes,
        size_t                      n_meshes,
        float                       margin_m);

/* Absolute pixel span of `grid` in ENU metres, expressed as positive
 * cell width (east) and cell height (north). The internal map->scale_n
 * is negative; this helper hides that. */
void osmmesh_planish_grid_cell_size(const osmmesh_terrain_grid *grid,
                                     const osmmesh_tile_enu_map *map,
                                     float *out_dx_m, float *out_dy_m);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_TERRAIN_PLANISH_H */
