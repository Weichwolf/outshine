/* libosmmesh/src/linear_internal.h
 *
 * Internal glue between linear.c (classification + orchestration) and
 * linear_ribbon.c (pure geometry sweep). Not installed.
 */

#ifndef OSMMESH_LINEAR_INTERNAL_H
#define OSMMESH_LINEAR_INTERNAL_H

#include <stdint.h>

#include "osmmesh/geo.h"
#include "osmmesh/linear.h"
#include "osmmesh/mesh.h"
#include "osmmesh/mvt.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "osmmesh/terrain.h"

/* Terrain-aware variant of osmmesh_linear_build.
 *
 * If `terrain_grid` is non-NULL, every emitted ribbon vertex has its z
 * set to `sample_bilinear(grid, xy) + clearance_m` (clearance = 0.05 m by
 * default). With terrain_grid == NULL the function is identical to
 * osmmesh_linear_build (z=0 plane).
 *
 * Normals are intentionally NOT recomputed: local slope angles from
 * terrain following are small (<~5 deg) and the visual consequence of
 * keeping (0,0,1) vertex normals is within MVP tolerance. */
int osmmesh_linear_build_with_terrain(
    const osmmesh_mvt_layer     *layer,
    const osmmesh_mvt_feature   *feature,
    const osmmesh_tile_enu_map  *map,
    const osmmesh_linear_opts   *opts,
    const osmmesh_terrain_grid  *terrain_grid,
    float                        clearance_m,
    osmmesh_mesh                *out_mesh,
    osmmesh_linear_info         *out_info);

typedef struct {
    float    *pos;
    float    *nrm;
    float    *uvs;    /* NULL iff UVs disabled */
    uint32_t *idx;
    uint32_t  v_count, v_cap;
    uint32_t  t_count, t_cap;
} osmmesh_linear_emit;

uint32_t osmmesh_linear_ribbon_vcap(uint32_t n_spine);
uint32_t osmmesh_linear_ribbon_tcap(uint32_t n_spine);

/* Emit one ribbon sub-string into the emit struct. Returns the total
 * sub-string length in metres (sum of edge lengths) or 0.0 if skipped
 * (too few vertices, zero total length, or OOM in a local scratch).
 * Advances e->v_count and e->t_count in-place. */
float osmmesh_linear_ribbon_emit(osmmesh_linear_emit *e,
                                  const float *xy, uint32_t n_spine,
                                  float width_m, float mitre_limit,
                                  int emit_uvs);

#ifdef __cplusplus
}
#endif

#endif
