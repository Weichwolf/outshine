/* Terrarium-RGB PNG tile -> float height grid -> triangulated ENU mesh. */

#ifndef OSMMESH_TERRAIN_H
#define OSMMESH_TERRAIN_H

#include <stdint.h>
#include <stddef.h>

#include "mesh.h"
#include "geo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Row-major, row 0 = north, col 0 = west; `heights` is owned (osmmesh_terrain_grid_free). */
typedef struct {
    float   *heights;
    uint32_t rows, cols;
} osmmesh_terrain_grid;

/* PNG must be RGB or RGBA (alpha ignored). On error `*out` is left zeroed and stbi_failure_reason()
 * goes to stderr — an unusual event worth preserving. */
int osmmesh_terrain_decode_png(const uint8_t *png_data, size_t png_len,
                                osmmesh_terrain_grid *out);

void osmmesh_terrain_grid_free(osmmesh_terrain_grid *grid);   /* safe on zeroed input */

/* stride must divide (rows-1) and (cols-1) exactly. */
typedef struct {
    uint32_t stride;           /* 1 = every pixel is a vertex */
    int      compute_normals;  /* 0 = skip, leave normals NULL */
    int      add_skirt;        /* reserved, must be 0 */
    float    skirt_depth_m;    /* reserved */
} osmmesh_terrain_build_opts;

/* Positions in ENU meters via `map`; fills *mesh with caller-owned buffers (osmmesh_mesh_free). */
int osmmesh_terrain_build_mesh(const osmmesh_terrain_grid *grid,
                                const osmmesh_tile_enu_map *map,
                                const osmmesh_terrain_build_opts *opts,
                                osmmesh_mesh *mesh);

/* The ECEF variant beside the ENU one: same heightfield/options/winding, but positions are per-vertex
 * float OFFSETS from `origin_ecef_out` (the tile centre, kept double) so a mesh can sit anywhere on
 * the planet. Normals are unit ECEF-axis, not world-up; no yoff lift — an ECEF vertex is absolute, so
 * the camera's own ECEF already carries its altitude. */
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
