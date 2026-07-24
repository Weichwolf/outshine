/* tiles/osmmesh/terrain.h -- server subset (T8 split, 2026-07-24).
 *
 * Terrarium-RGB PNG tile -> float height grid. fb-tiles' elev.c decodes exactly this (per-point
 * elevation lookups for /elev), never a mesh -- meshing is a client/rendering concern that stays
 * in the client's own osmmesh copy (temp/geo/osmmesh, full terrain.h incl. build_mesh/build_mesh_
 * ecef). See osmmesh.h for why this file is a deliberately smaller copy, not a re-export.
 *
 *   osmmesh_terrain_decode_png:  decode a PNG payload to a float grid.
 *                                Each pixel's 24-bit RGB is interpreted per
 *                                the Mapzen / AWS Terrarium spec:
 *                                    h = R*256 + G + B/256 - 32768   [m]
 *
 * Grid orientation: row 0 is the NORTH (top) edge, col 0 the WEST (left)
 * edge. This matches PNG layout and the MVT/slippy-tile convention that
 * local_y grows south.
 */

#ifndef OSMMESH_TERRAIN_H
#define OSMMESH_TERRAIN_H

#include <stdint.h>
#include <stddef.h>

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

#define OSMMESH_TERRAIN_OK            0
#define OSMMESH_TERRAIN_ERR_DECODE   -1   /* PNG broken / wrong channels */
#define OSMMESH_TERRAIN_ERR_ARG      -2   /* NULL / stride-doesn't-divide */
#define OSMMESH_TERRAIN_ERR_OOM      -3

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_TERRAIN_H */
