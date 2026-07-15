/* libosmmesh/src/building_geom.h
 *
 * Shared 2-D polygon utilities for the T15+ roof builders. Ports the
 * geometry-only subset of streetwalk/tools/building/builders/roofs/_geom.py
 * into C99. Public to libosmmesh internals only — do NOT move under
 * include/osmmesh/.
 *
 * The builders under src/ share these primitives so every roof algorithm
 * agrees on OBB orientation, signed-area convention, and cut semantics.
 * Mesh emission stays in the individual builder TUs; this file is pure
 * geometry.
 */

#ifndef OSMMESH_BUILDING_GEOM_H
#define OSMMESH_BUILDING_GEOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * Oriented bounding box
 * ------------------------------------------------------------------------ */

typedef struct {
    float center[2];     /* OBB center (xy). */
    float axis_x[2];     /* unit long-axis vector. */
    float axis_y[2];     /* unit short-axis vector (perpendicular to axis_x). */
    float extent[2];     /* full lengths along axis_x and axis_y respectively. */
} osmmesh_building_obb_t;

/* Compute the minimum-area oriented bounding box of a 2-D polygon via
 * rotating calipers over the convex hull. Output `axis_x` is the
 * longer-side unit direction, `extent[0]` >= `extent[1]`.
 *
 * Returns 0 on success, -1 on degenerate input (n < 3 or fully collinear).
 * `xy` is interleaved [x0,y0,x1,y1,...] with `n` vertices. */
int osmmesh_building_geom_obb(const float *xy, uint32_t n,
                               osmmesh_building_obb_t *out);

/* ------------------------------------------------------------------------
 * Polygon properties
 * ------------------------------------------------------------------------ */

/* Shoelace signed area. Positive for CCW rings, negative for CW. */
float osmmesh_building_geom_signed_area(const float *xy, uint32_t n);

/* Convenience: 1 if signed area > 0, else 0. */
int osmmesh_building_geom_is_ccw(const float *xy, uint32_t n);

/* Rectangle test. Returns 1 iff `xy` is a 4-vertex rectangle whose edges
 * are parallel to `axis_dir` (unit 2-vector) or its perpendicular. The
 * tolerance (0.02 m^2 of off-axis projection) absorbs the few cm of
 * OSM digitising noise typical of Shortbread footprints. */
int osmmesh_building_geom_is_axis_aligned_rect(const float *xy, uint32_t n,
                                                 const float axis_dir[2]);

/* ------------------------------------------------------------------------
 * Polygon split by infinite line
 * ------------------------------------------------------------------------ */

/* Split a simple polygon along the infinite line through `origin` with
 * direction `dir` (need not be normalised, must be non-zero). Writes two
 * malloc'd sub-polygons into (*out_a, *out_an) and (*out_b, *out_bn),
 * each as an interleaved [x,y,...] array; caller frees with free().
 *
 * Returns 0 iff the cut yields exactly two sub-polygons (both with >= 3
 * vertices). Returns -1 on any other outcome — line misses, grazes a
 * vertex, or produces more than two pieces (non-convex polygon cut
 * across a concavity). On failure `*out_a`/`*out_b` are set to NULL. */
int osmmesh_building_geom_polygon_split(const float *xy, uint32_t n,
                                          const float origin[2],
                                          const float dir[2],
                                          float **out_a, uint32_t *out_an,
                                          float **out_b, uint32_t *out_bn);

/* ------------------------------------------------------------------------
 * Roof-height helpers
 * ------------------------------------------------------------------------ */

/* Apex height for a given horizontal run and pitch angle. `pitch_deg` is
 * the rise/run angle in degrees. Returns run_m * tan(pitch_deg). */
float osmmesh_building_geom_ridge_height_for_pitch(float run_m,
                                                    float pitch_deg);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_BUILDING_GEOM_H */
