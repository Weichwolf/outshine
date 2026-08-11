#ifndef OSMMESH_OSMMESH_H
#define OSMMESH_OSMMESH_H

/* Terrain-only osmmesh: fetch Terrarium DEM tiles through a host provider, decode, edge-stitch, mesh.
 * The vector/building/MVT/PMTiles machinery of the full library is out of scope. */

#include "geo.h"
#include "mesh.h"
#include "terrain.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct osmmesh_ctx osmmesh_ctx;

typedef enum {
    OSMMESH_TILE_VECTOR  = 0,
    OSMMESH_TILE_TERRAIN = 1    /* Terrarium-encoded PNG */
} osmmesh_tile_kind;

/* 1 hands over a malloc'd buffer (osmmesh frees it), 0 = absent OR not yet fetched — both are "no
 * tile". MUST NOT BLOCK: on WASM the host answers from its own async cache. */
typedef int (*osmmesh_tile_provider)(void *user, osmmesh_tile_kind kind,
                                     uint32_t z, uint32_t x, uint32_t y,
                                     uint8_t **out, size_t *len);

typedef struct {
    osmmesh_tile_provider  tile_provider;
    void                  *tile_provider_user;

    /* Requests above it step up to the parent tile and crop; 0 = every zoom is served. */
    int                    provider_terrain_max_zoom;

    /* Origin of the shared ENU frame every mesh is projected into. */
    double origin_lat;
    double origin_lon;

    /* NULL = generator defaults. */
    const osmmesh_terrain_build_opts *terrain_opts;

    /* Decoded source grids held against the next stitch, 0 = the built-in ceiling. A context that
     * only asks about single tiles pays 256 KiB per slot for a cache it cannot use. */
    int  dem_cache_tiles;

    int  enable_terrain;
} osmmesh_config;

typedef struct {
    uint8_t  z;
    uint32_t x, y;

    /* Owned; NULL if disabled or the tile is missing. */
    osmmesh_mesh *terrain;
} osmmesh_tile;

#define OSMMESH_OK               0
#define OSMMESH_ERR_ARG         -1
#define OSMMESH_ERR_IO          -2
#define OSMMESH_ERR_DECODE      -3
#define OSMMESH_ERR_OOM         -4
#define OSMMESH_ERR_CONFIG      -5

int  osmmesh_create(const osmmesh_config *cfg, osmmesh_ctx **out);
void osmmesh_destroy(osmmesh_ctx *ctx);

/* An ABSENT tile returns OK with terrain == NULL: absence is not an error at this layer. Only decode
 * failures and OOM return negative codes. */
int  osmmesh_fetch_tile(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                        osmmesh_tile *out);

/* Safe on a zero-init struct and safe twice. */
void osmmesh_free_tile(osmmesh_tile *tile);

/* THE HEIGHT FIELD THE TILE'S MESH IS BUILT FROM: cropped from the parent past the provider's max
 * zoom, then edge-averaged against its four neighbours. Borrowed — it lives until the next call on
 * this context. `postings` is the posting count per edge under this context's stride, so a caller
 * cannot land on a lattice the builder does not use.
 *
 * The STATUS is the return value, exactly as osmmesh_fetch_tile: OK with *out_grid == NULL means the
 * bytes are not here (absent, or not fetched yet), and an OOM says so instead of arriving at the
 * caller as a tile without a DEM. */
int osmmesh_tile_grid(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                      const osmmesh_terrain_grid **out_grid,
                      uint32_t *row_postings, uint32_t *col_postings);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_OSMMESH_H */
