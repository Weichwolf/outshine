/* libosmmesh/include/osmmesh/building.h
 *
 * Building-mesh generator (T6, MVP). Turns one MVT polygon feature into a
 * triangulated mesh of walls + roof in ENU meters, using a seeded RNG (seed
 * = osm_id) to pick massing details when OSM tags leave them unspecified.
 *
 * Scope (MVP):
 *   - Three roof shapes: FLAT, GABLED, HIPPED. Gabled / hipped only fire on
 *     rectangular footprints (hull-ratio > 0.95 && n_corners == 4). Anything
 *     else falls back to FLAT.
 *   - Inner rings (courtyards) are ignored: one stderr line then skipped.
 *   - No classification taxonomy; footprint shape + OSM tags + RNG only.
 *
 * Winding / orientation (do not "just flip until green" — see building.c):
 *   MVT outer rings are CW in screen-space (y down). After y-flip into ENU
 *   (scale_n < 0) the same polygon is CW with respect to the right-handed
 *   ENU basis (E right, N up). Walls and roof use that CW order consistently:
 *
 *     wall outward normal  =  edge_dir x up     (CW ring → points outward)
 *     roof top-face normal =  (p1-p0) x (p2-p0) reversed on CW rings so it
 *                             points +Z.
 *
 * See building.c for the algebraic derivation.
 */

#ifndef OSMMESH_BUILDING_H
#define OSMMESH_BUILDING_H

#include <stdint.h>

#include "osmmesh/geo.h"
#include "osmmesh/mesh.h"
#include "osmmesh/mvt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OSMMESH_ROOF_FLAT   = 0,
    OSMMESH_ROOF_GABLED = 1,
    OSMMESH_ROOF_HIPPED = 2,
    OSMMESH_ROOF_COUNT
} osmmesh_roof_shape;

typedef struct {
    /* Floor height when only `building:levels` is present.
     * Typical: 3.0 m (residential), used uniformly in MVP. */
    float floor_height_m;

    /* Override knobs for tests and caller control. 0 / not-set = auto. */
    float              wall_height_override_m;
    osmmesh_roof_shape roof_shape_override;
    int                roof_shape_override_set;
    float              roof_height_override_m;
} osmmesh_building_opts;

typedef struct {
    osmmesh_roof_shape roof_shape;       /* chosen shape */
    float              wall_height_m;    /* eaves height */
    float              roof_height_m;    /* top above eaves */
    float              footprint_area_m2;
    float              obb_long_m, obb_short_m;
    uint32_t           footprint_vertices;
    uint64_t           osm_id;
    int                is_rectangular;   /* hull-ratio > 0.95 && n_corners == 4 */
    int                is_regular;       /* hull-ratio > 0.85 */
} osmmesh_building_info;

/* Build a building mesh from one MVT polygon feature.
 *
 *   layer, feature — MVT source (T3 decoder). Must be POLYGON.
 *   map            — tile -> ENU mapping (T4).
 *   opts           — NULL = use defaults (floor_height_m=3, no overrides).
 *   out_mesh       — filled on success, caller owns via osmmesh_mesh_free.
 *   out_info       — nullable debug info.
 *
 * Returns 0 on success, negative on error.
 */
int osmmesh_building_build(const osmmesh_mvt_layer    *layer,
                            const osmmesh_mvt_feature *feature,
                            const osmmesh_tile_enu_map *map,
                            const osmmesh_building_opts *opts,
                            osmmesh_mesh              *out_mesh,
                            osmmesh_building_info     *out_info);

#define OSMMESH_BUILDING_OK         0
#define OSMMESH_BUILDING_ERR_SKIP  -1   /* zero-area / <3 vertices / no outer */
#define OSMMESH_BUILDING_ERR_ARG   -2
#define OSMMESH_BUILDING_ERR_OOM   -3

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_BUILDING_H */
