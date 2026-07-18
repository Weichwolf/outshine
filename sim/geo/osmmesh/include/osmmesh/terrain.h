/* libosmmesh/include/osmmesh/terrain.h
 *
 * Terrarium-RGB PNG tile -> float height grid -> triangulated mesh.
 *
 *   osmmesh_terrain_decode_png:  decode a PNG payload to a float grid.
 *                                Each pixel's 24-bit RGB is interpreted per
 *                                the Mapzen / AWS Terrarium spec:
 *                                    h = R*256 + G + B/256 - 32768   [m]
 *
 *   osmmesh_terrain_build_mesh:  build a regular-grid mesh in ENU meters,
 *                                using the T4 tile_enu_map for the horizontal
 *                                placement and the grid heights for the Up
 *                                component. Optionally computes per-vertex
 *                                normals (area-weighted average of adjacent
 *                                face normals).
 *
 * Grid orientation: row 0 is the NORTH (top) edge, col 0 the WEST (left)
 * edge. This matches PNG layout and the MVT/slippy-tile convention that
 * local_y grows south. Build_mesh places vertex (r,c) at local tile
 * coordinate (c*dx, r*dy) with dx = extent/(cols-1), dy = extent/(rows-1);
 * since `map->scale_n` is already negative (ENU n grows north while r/
 * local_y grow south) the output positions come out ENU-correct without any
 * further sign flip.
 */

#ifndef OSMMESH_TERRAIN_H
#define OSMMESH_TERRAIN_H

#include <stdint.h>
#include <stddef.h>

#include "osmmesh/mesh.h"
#include "osmmesh/geo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded heightfield. Row-major, row 0 = TOP (north), col 0 = LEFT (west).
 * Heights in meters. `heights` is owned; free via osmmesh_terrain_grid_free. */
typedef struct {
    float   *heights;
    uint32_t rows, cols;
} osmmesh_terrain_grid;

/* Decode a PNG payload (bytes as stored in the Terrarium-RGB tile) into a
 * float grid. PNG must be RGB or RGBA (alpha is ignored). Returns
 * OSMMESH_TERRAIN_OK on success, negative error code otherwise. On error
 * `*out` is left zeroed.
 *
 * A human-readable stbi_failure_reason() is emitted to stderr on decode
 * failure -- this is an unusual event worth preserving. */
int osmmesh_terrain_decode_png(const uint8_t *png_data, size_t png_len,
                                osmmesh_terrain_grid *out);

/* Free a grid's heights buffer and zero the struct. Safe on zeroed input. */
void osmmesh_terrain_grid_free(osmmesh_terrain_grid *grid);

/* Mesh build options. stride must divide (rows-1) and (cols-1) exactly;
 * the resulting mesh has (rows-1)/stride + 1 rows and (cols-1)/stride + 1
 * columns of vertices. add_skirt is reserved; must be 0 for MVP. */
typedef struct {
    uint32_t stride;           /* 1 = every pixel is a vertex */
    int      compute_normals;  /* 0 = skip, leave normals NULL */
    int      add_skirt;        /* reserved, must be 0 */
    float    skirt_depth_m;    /* reserved */
} osmmesh_terrain_build_opts;

/* Build a triangulated mesh from a heightfield. Positions in ENU meters
 * using `map`. Returns OSMMESH_TERRAIN_OK on success, fills *mesh with
 * caller-owned buffers (release via osmmesh_mesh_free). On error the
 * mesh is left zeroed. */
int osmmesh_terrain_build_mesh(const osmmesh_terrain_grid *grid,
                                const osmmesh_tile_enu_map *map,
                                const osmmesh_terrain_build_opts *opts,
                                osmmesh_mesh *mesh);

/* ------------------------------------------------------------------------
 * ECEF terrain mesh (global-scale path, added ALONGSIDE build_mesh above).
 *
 * Same heightfield, same stride/normal/UV options, same triangle winding --
 * but positions are WGS84 ECEF instead of flat-earth ENU, so the tile sits on
 * the real curved ellipsoid and a mesh can be placed anywhere on the planet.
 *
 * Output contract (what the renderer consumes, step 2):
 *   - origin_ecef_out[3]  : a per-tile local origin in ECEF meters (double).
 *                           The tile CENTER, projected to the ellipsoid at the
 *                           tile-center terrain height. Keep it as double.
 *   - mesh->positions[]   : per-vertex offset FROM origin_ecef_out, in meters,
 *                           on ECEF axes, stored float. Small (|offset| ~ half
 *                           a tile diagonal + local relief, << 2 km at z14) so
 *                           float holds them to sub-cm.
 *   - mesh->normals[]     : unit ECEF-axis normals (roughly radial-outward plus
 *                           relief; NOT world-up). NULL if compute_normals==0.
 *   - mesh->uvs[]         : identical to the ENU path.
 *   - mesh->indices[]     : identical winding to the ENU path (CCW seen from
 *                           outside the ellipsoid -> normals point outward).
 *
 * Per frame the renderer computes cam_ecef (double) from the aircraft geodetic
 * pose, then per tile trans = origin_ecef_out - cam_ecef (double subtract ->
 * float) and draws with the camera at the origin. There is NO yoff lift here:
 * the ENU path lifts the camera by the origin ground elevation because its
 * vertices are relative to a ground-anchored plane; ECEF vertices are absolute
 * on the ellipsoid, so the camera's own ECEF already carries its altitude.
 *
 * z/x/y select the slippy tile; per-vertex lon/lat come from
 * osmmesh_tile_frac_to_geo (full precision), height from the grid.
 * ------------------------------------------------------------------------ */
int osmmesh_terrain_build_mesh_ecef(const osmmesh_terrain_grid *grid,
                                     uint8_t z, uint32_t x, uint32_t y,
                                     const osmmesh_terrain_build_opts *opts,
                                     osmmesh_mesh *mesh,
                                     double origin_ecef_out[3]);

#define OSMMESH_TERRAIN_OK            0
#define OSMMESH_TERRAIN_ERR_DECODE   -1   /* PNG broken / wrong channels */
#define OSMMESH_TERRAIN_ERR_ARG      -2   /* NULL / stride-doesn't-divide */
#define OSMMESH_TERRAIN_ERR_OOM      -3

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_TERRAIN_H */
