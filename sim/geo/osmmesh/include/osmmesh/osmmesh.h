/* libosmmesh/include/osmmesh/osmmesh.h
 *
 * Top-level meta-header for the osmmesh library (T8).
 *
 * Use this single header when you want "everything in one include". The
 * sub-headers (pmtiles.h / mvt.h / geo.h / mesh.h / terrain.h / building.h /
 * linear.h) remain directly includable for callers who prefer decomposition.
 *
 *   #include "osmmesh/osmmesh.h"
 *
 *       osmmesh_ctx   *ctx = NULL;
 *       osmmesh_config cfg = { .vector_url  = "hameln.pmtiles",
 *                              .terrain_url = "hameln_terrain.pmtiles",
 *                              .origin_lat  = 52.045,
 *                              .origin_lon  =  9.385 };
 *       osmmesh_create(&cfg, &ctx);
 *
 *       osmmesh_tile tile = {0};
 *       osmmesh_fetch_tile(ctx, 14, 8619, 5408, &tile);
 *           // tile.terrain, tile.buildings[0..n_buildings), tile.linears[...]
 *       osmmesh_free_tile(&tile);
 *
 *       osmmesh_destroy(ctx);
 *
 * The context opens the PMTiles archive(s) up front, parses the header, and
 * caches an ENU origin frame. Keep it alive across many fetches; it is not
 * thread-safe -- use one context per thread, or serialise externally.
 */

#ifndef OSMMESH_H
#define OSMMESH_H

#include "osmmesh/pmtiles.h"
#include "osmmesh/mvt.h"
#include "osmmesh/geo.h"
#include "osmmesh/mesh.h"
#include "osmmesh/terrain.h"
#include "osmmesh/building.h"
#include "osmmesh/linear.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct osmmesh_ctx osmmesh_ctx;

typedef struct {
    /* Vector-tile source. The MVP accepts local filesystem paths only.
     * The field is named "url" because T10 will add an HTTP IO backend
     * to the PMTiles reader and route URL-shaped strings through it;
     * the integration layer will pick the backend based on the prefix. */
    const char *vector_url;

    /* Optional terrain source. NULL => skip terrain in output bundles. */
    const char *terrain_url;

    /* Alternative to the file-path URLs: pre-loaded byte buffers. If set
     * (non-NULL, len > 0), the memory-backed PMTiles IO is used instead
     * of opening a file. The context borrows the buffers; the caller must
     * keep them alive until osmmesh_destroy. Added in T10 so the WASM
     * build can hand fetch()'d ArrayBuffers directly to the reader without
     * going through Emscripten's MEMFS. Rules:
     *   - vector_data != NULL  overrides  vector_url
     *   - terrain_data != NULL overrides  terrain_url
     *   - mixing (e.g. memory vector + file terrain) is allowed */
    const uint8_t *vector_data;
    size_t         vector_len;
    const uint8_t *terrain_data;
    size_t         terrain_len;

    /* Origin for ENU projection. Typically the BBox centre of the region
     * the caller is streaming; all meshes share this frame so the consumer
     * can stitch them without re-projecting. */
    double origin_lat;
    double origin_lon;

    /* Per-feature generator knobs. NULL => use the generator's defaults. */
    const osmmesh_building_opts       *building_opts;
    const osmmesh_linear_opts         *linear_opts;
    const osmmesh_terrain_build_opts  *terrain_opts;

    /* Layer selection. If any flag is 0, the whole category is skipped.
     * Zero-init = all disabled; set to 1 to enable. Pass `{.enable_* = 1}`
     * designated initialisers for the common "enable everything" path. */
    int  enable_terrain;
    int  enable_buildings;
    int  enable_linears;
} osmmesh_config;

typedef struct {
    uint8_t  z;
    uint32_t x, y;

    /* Terrain: single mesh, or NULL if disabled / tile missing. Owned. */
    osmmesh_mesh *terrain;

    /* Buildings: parallel arrays, `n_buildings` entries each. NULL when
     * n_buildings == 0. All contained meshes are owned (freed by
     * osmmesh_free_tile). */
    osmmesh_mesh          *buildings;
    osmmesh_building_info *building_infos;
    uint32_t               n_buildings;

    /* Linears: parallel arrays, `n_linears` entries each. */
    osmmesh_mesh        *linears;
    osmmesh_linear_info *linear_infos;
    uint32_t             n_linears;

    /* Diagnostics: features that classified as UNKNOWN, had zero area,
     * fewer than the minimum vertex count, etc. Useful as a cheap LOD
     * heuristic signal in future work. */
    uint32_t n_skipped_features;
} osmmesh_tile;

/* Create a context from config. Opens both archives, pre-reads root dirs,
 * caches an ENU origin frame. Single-threaded; keep alive for many fetches.
 *
 * Returns OSMMESH_OK or a negative error code. On failure *out is left NULL. */
int  osmmesh_create(const osmmesh_config *cfg, osmmesh_ctx **out);
void osmmesh_destroy(osmmesh_ctx *ctx);

/* Fetch and build one tile. Returns OSMMESH_OK on success; the returned
 * `osmmesh_tile` may still have zero buildings/linears if no features matched.
 *
 * Tiles absent from the archive return OSMMESH_OK with empty counts and
 * `terrain == NULL` — absence is not an error at this layer (callers that
 * stream at tile granularity observe many misses at the region edges and
 * do not want to log each one).
 *
 * Hard errors (corrupt archive, decode failure, OOM) return negative codes. */
int  osmmesh_fetch_tile(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                         osmmesh_tile *out);

/* Free all owned data in `tile` (including the inner meshes) and zero the
 * struct. Safe on a zero-initialised struct; safe to call twice. */
void osmmesh_free_tile(osmmesh_tile *tile);

#define OSMMESH_OK               0
#define OSMMESH_ERR_ARG         -1
#define OSMMESH_ERR_IO          -2
#define OSMMESH_ERR_DECODE      -3
#define OSMMESH_ERR_OOM         -4
#define OSMMESH_ERR_CONFIG      -5

/* =======================================================================
 *  T10: JS-binding helpers. Small accessors that let an Emscripten/JS
 *  wrapper read struct fields without duplicating the C layout. Returned
 *  pointers alias memory owned by the tile and are valid until
 *  osmmesh_free_tile / osmmesh_tile_free_wrap releases it.
 *
 *  Native code has direct struct access; these exist only to widen the
 *  ABI for foreign callers.
 * ===================================================================== */

uint32_t  osmmesh_mesh_n_vertices_get  (const osmmesh_mesh *m);
uint32_t  osmmesh_mesh_n_triangles_get (const osmmesh_mesh *m);
float    *osmmesh_mesh_positions_get   (const osmmesh_mesh *m);
float    *osmmesh_mesh_normals_get     (const osmmesh_mesh *m);
float    *osmmesh_mesh_uvs_get         (const osmmesh_mesh *m);
uint32_t *osmmesh_mesh_indices_get     (const osmmesh_mesh *m);

osmmesh_mesh *osmmesh_tile_terrain_get   (const osmmesh_tile *t);
uint32_t      osmmesh_tile_n_buildings_get(const osmmesh_tile *t);
uint32_t      osmmesh_tile_n_linears_get  (const osmmesh_tile *t);
uint32_t      osmmesh_tile_n_skipped_get  (const osmmesh_tile *t);
osmmesh_mesh *osmmesh_tile_building_at   (const osmmesh_tile *t, uint32_t i);
osmmesh_mesh *osmmesh_tile_linear_at     (const osmmesh_tile *t, uint32_t i);
/* Per-feature kind getter for the JS bridge — returns the int value of the
 * osmmesh_linear_kind enum so colour mapping can happen client-side. -1 on
 * out-of-range. */
int           osmmesh_tile_linear_kind_at(const osmmesh_tile *t, uint32_t i);

/* Heap-allocate an osmmesh_tile (calloc'd). Pair with osmmesh_tile_free_wrap;
 * the latter calls osmmesh_free_tile internally before releasing the struct. */
osmmesh_tile *osmmesh_tile_alloc   (void);
void          osmmesh_tile_free_wrap(osmmesh_tile *t);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_H */
