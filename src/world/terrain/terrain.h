/* Terrarium-RGB PNG tile -> float height grid -> triangulated ENU mesh. */

#ifndef OSMMESH_TERRAIN_H
#define OSMMESH_TERRAIN_H

#include <stdint.h>
#include <stddef.h>

#include "mesh.h"
#include "geo.h"
#include "tilemath.h"   /* fb-tiles' OWN registration: the postings below and /elev share one line */

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
    uint32_t stride;           /* 1 = every pixel is a node */
} osmmesh_terrain_build_opts;

/* Postings per tile edge for a given source edge and stride; the stride must divide (edge - 1). */
static inline uint32_t osmmesh_terrain_postings(uint32_t src_edge, uint32_t stride)
{ return (src_edge - 1u) / stride + 1u; }

/* THE POSTING LATTICE. Posting k of n sits on this tile fraction, and the mesh's vertex, its uv and
 * the height oracle all take it from here — an lattice restated anywhere else drifts in the last bit
 * and the oracle stops standing on the drawn triangle. */
static inline double osmmesh_terrain_posting_frac(uint32_t k, uint32_t n)
{ return (double)k * (1.0 / (double)(n - 1u)); }

/* A texel is an AREA, not a lattice point (tilemath.h), so a posting is INTERPOLATED and never
 * indexed. Height in metres, on the source grid's own datum. */
static inline float osmmesh_terrain_posting_height(const osmmesh_terrain_grid *grid,
                                                    double frac_col, double frac_row)
{
    return fb_bilinear(grid->heights, grid->cols, grid->rows,
                       fb_texel_index(frac_col, grid->cols),
                       fb_texel_index(frac_row, grid->rows));
}

/* Positions in ENU meters via `map`; fills *mesh with caller-owned buffers (osmmesh_mesh_free). */
int osmmesh_terrain_build_mesh(const osmmesh_terrain_grid *grid,
                                const osmmesh_tile_enu_map *map,
                                const osmmesh_terrain_build_opts *opts,
                                osmmesh_mesh *mesh);

#define OSMMESH_TERRAIN_OK            0
#define OSMMESH_TERRAIN_ERR_DECODE   -1   /* PNG broken / wrong channels */
#define OSMMESH_TERRAIN_ERR_ARG      -2   /* NULL / stride-doesn't-divide */
#define OSMMESH_TERRAIN_ERR_OOM      -3

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_TERRAIN_H */
