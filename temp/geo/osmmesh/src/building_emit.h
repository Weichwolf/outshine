/* libosmmesh/src/building_emit.h
 *
 * Shared mesh-emission helpers for T15+ roof builders. Ports
 * streetwalk/tools/building/builders/roofs/_emit.py into C99.
 *
 * Ownership:
 *   The emit buffer `osmmesh_emit_buf` owns its positions/normals/indices
 *   arrays. Walls and roof are emitted into the SAME buffer by
 *   building_roof.c's dispatch — that is the whole point of this module:
 *   walls and the bordering roof face share eave-edge vertex coordinates
 *   by construction, so the closed-mesh invariant (test_087) holds without
 *   a downstream "stitch-by-position" pass.
 *
 * Coordinate convention:
 *   Input 2-D polygon arrays are interleaved [x0,y0,x1,y1,...] in ENU
 *   meters (x = east, y = north). The world is Z-up; vertical coordinates
 *   are passed as separate per-vertex `y_*` arrays (the names echo the
 *   Python port where y was the vertical axis). We re-label them as z in
 *   stored positions.
 *
 * Private to libosmmesh — NOT part of the public API.
 */

#ifndef OSMMESH_BUILDING_EMIT_H
#define OSMMESH_BUILDING_EMIT_H

#include <stdint.h>

#include "building_skeleton.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Resizable vertex/index buffer
 * ------------------------------------------------------------------------- */

typedef struct {
    float    *positions;    /* nv * 3 floats (x,y,z) */
    float    *normals;      /* nv * 3 floats */
    uint32_t *indices;      /* nt * 3 uint32 */
    uint32_t  nv, nv_cap;
    uint32_t  nt, nt_cap;   /* triangle count, NOT index count */
} osmmesh_emit_buf;

void osmmesh_emit_buf_init(osmmesh_emit_buf *b);
void osmmesh_emit_buf_free(osmmesh_emit_buf *b);

/* Ensure capacity for at least `nv_min` more vertices and `nt_min` more
 * triangles. Grows by doubling when needed. Returns 0 on success, -1 on OOM. */
int osmmesh_emit_buf_reserve(osmmesh_emit_buf *b,
                              uint32_t extra_nv, uint32_t extra_nt);

/* -------------------------------------------------------------------------
 * Emit primitives
 * ------------------------------------------------------------------------- */

/* Ear-clip a closed ring on its xy footprint and emit each triangle with
 * a per-vertex z value. `xy` is interleaved [x0,y0,...] (ENU horizontal),
 * `z_per_vertex` supplies the vertical for each ring vertex; the ring is
 * lifted linearly across each triangle. `ccw` denotes the source ring's
 * orientation — winding is picked so every emitted face normal has nz >= 0
 * (roof faces point skyward, backface culling sees the roof from above).
 *
 * Returns 0 on success, -1 on OOM. Silently skips n<3 or degenerate input. */
int osmmesh_emit_lifted_ring(osmmesh_emit_buf *b,
                              const float *xy, uint32_t n,
                              const float *z_per_vertex,
                              int ccw);

/* Emit the below-eaves vertical walls of a building: one quad per
 * footprint edge, from `z_bot_per_v[i]` up to the flat eaves plane at
 * `y_eaves`. Matches streetwalk's builders/extrude.py::_walls, extended
 * with per-vertex bottoms for terrain drape.
 *
 * `ccw` is the source-ring orientation; the outward normal and triangle
 * winding are picked to agree with it.
 *
 * Skips edges of zero length. Returns 0 on success, -1 on OOM.
 */
int osmmesh_emit_walls_below_eaves(osmmesh_emit_buf *b,
                                     const float *xy, uint32_t n,
                                     const float *z_bot_per_v,
                                     float        y_eaves,
                                     int ccw);

/* Emit above-eaves gable-end fill for a ring. For each edge, fills the
 * gap between the flat eaves contour (at y_eaves) and the per-vertex
 * lifted height (z_top_per_v[i], z_top_per_v[j]) — a trapezoid above
 * the eaves plane. Direct port of streetwalk's _emit.py::emit_wall_fill.
 *
 * Edges both of whose endpoints sit at y_eaves are skipped (no gable).
 *
 * Cut line: when `cut_origin` + `cut_dir` are non-NULL, edges crossing
 * the cut are emitted as a single triangle reaching up to `y_ridge` at
 * the intersection point. This closes the seam between gable wall and
 * roof ridge vertex by construction.
 *
 * Returns 0 on success, -1 on OOM.
 */
int osmmesh_emit_wall_fill(osmmesh_emit_buf *b,
                            const float *xy, uint32_t n,
                            const float *z_top_per_v,
                            int ccw,
                            const float *cut_origin,
                            const float *cut_dir,
                            float        y_eaves,
                            float        y_ridge);

/* Emit every face of a straight skeleton as a lifted, ear-clipped ring.
 *
 *   sk           : skeleton (building_skeleton.h). t ranges 0..t_max.
 *   footprint_xy : original footprint ring (interleaved xy). Used to
 *                  SNAP t=0 skeleton vertices back to the exact float
 *                  coordinate of the matching footprint vertex — the
 *                  skeleton's centre-then-translate pipeline loses up
 *                  to a cm of precision, which otherwise desyncs the
 *                  skeleton's eave edges from the adjacent wall top.
 *                  Pass NULL to skip snapping.
 *   footprint_n  : number of footprint vertices. Ignored when
 *                  footprint_xy is NULL.
 *   y_eaves      : z at t=0 (wall-top plane).
 *   y_ridge      : z at t=t_max (roof apex).
 *
 * A no-op when sk->t_max < 1e-6. Returns 0 on success, -1 on OOM.
 */
int osmmesh_emit_skeleton_faces(osmmesh_emit_buf *b,
                                 const osmmesh_skeleton *sk,
                                 const float *footprint_xy,
                                 uint32_t footprint_n,
                                 float y_eaves, float y_ridge);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_BUILDING_EMIT_H */
