#ifndef OSMMESH_OSMMESH_H
#define OSMMESH_OSMMESH_H

/* Terrain-only osmmesh: fetches Terrarium-encoded DEM tiles through a host
 * provider, decodes + edge-stitches them, and builds a per-tile ENU terrain
 * mesh. The vector/building/linear/MVT/PMTiles machinery of full osmmesh is
 * out of scope here; FlightBox streams terrain only. */

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

/* Host tile byte source. Returns 1 and hands over a malloc'd buffer in *out
 * (+ *len, freed by osmmesh with free()); returns 0 when the tile is absent or
 * not yet fetched (both are treated as "no tile"). MUST NOT BLOCK: on WASM the
 * host fetches asynchronously and answers from its own cache, returning 0 until
 * the bytes land. */
typedef int (*osmmesh_tile_provider)(void *user, osmmesh_tile_kind kind,
                                     uint32_t z, uint32_t x, uint32_t y,
                                     uint8_t **out, size_t *len);

typedef struct {
    osmmesh_tile_provider  tile_provider;
    void                  *tile_provider_user;

    /* Highest terrain zoom the provider serves. Requests above it step up to
     * the parent tile and crop. 0 => provider serves every requested zoom. */
    int                    provider_terrain_max_zoom;

    /* Origin for the shared ENU frame all meshes are projected into. */
    double origin_lat;
    double origin_lon;

    /* Terrain mesh generator knobs. NULL => generator defaults. */
    const osmmesh_terrain_build_opts *terrain_opts;

    int  enable_terrain;
} osmmesh_config;

typedef struct {
    uint8_t  z;
    uint32_t x, y;

    /* Terrain: single owned mesh, or NULL if disabled / tile missing. Freed by
     * osmmesh_free_tile. */
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

/* Fetch + decode + stitch + mesh one tile. Absent tiles return OSMMESH_OK with
 * terrain == NULL (absence is not an error at this layer). Hard errors (decode
 * failure, OOM) return negative codes. */
int  osmmesh_fetch_tile(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                        osmmesh_tile *out);

/* Free the owned mesh in `tile` and zero it. Safe on zero-init and twice. */
void osmmesh_free_tile(osmmesh_tile *tile);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_OSMMESH_H */
