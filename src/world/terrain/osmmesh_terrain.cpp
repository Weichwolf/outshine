#include "osmmesh.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The raw fetch is hit ~15x per output tile (self + 4 stitch neighbours, each re-stitching), so
 * caching the DECODED grid short-circuits the dominant cold-boot cost: the PNG decode. */
#define OM_DEM_LRU_CAP 128
typedef struct { uint64_t seq; int used; uint8_t z; uint32_t x, y; osmmesh_terrain_grid g; } om_dem_ent;

struct osmmesh_ctx {
    osmmesh_enu_ctx enu;      /* origin frame for all ENU conversions */

    osmmesh_tile_provider tile_provider;
    void                 *tile_provider_user;
    int                   provider_terrain_max_zoom;

    osmmesh_terrain_build_opts terrain_opts;
    int                        has_terrain_opts;
    int                        enable_terrain;

    om_dem_ent dem_lru[OM_DEM_LRU_CAP];
    int        dem_cap;
    uint64_t   dem_seq;

    osmmesh_terrain_grid borrowed;   /* what osmmesh_tile_grid last handed out */
};

/* Absent or not-yet-fetched both give 0. */
static int om_tile_bytes(osmmesh_ctx *ctx, osmmesh_tile_kind kind,
                         uint32_t z, uint32_t x, uint32_t y, uint8_t **out, size_t *len)
{
    *out = NULL; *len = 0;
    if (!ctx->tile_provider) return 0;
    return ctx->tile_provider(ctx->tile_provider_user, kind, z, x, y, out, len) ? 1 : 0;
}

static int om_dem_lru_get(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                          osmmesh_terrain_grid *out)
{
    for (int i = 0; i < ctx->dem_cap; i++) {
        om_dem_ent *e = &ctx->dem_lru[i];
        if (!e->used || e->z != z || e->x != x || e->y != y) continue;
        e->seq = ++ctx->dem_seq;                         /* touch */
        size_t n = (size_t)e->g.rows * e->g.cols;
        out->rows = e->g.rows; out->cols = e->g.cols; out->heights = NULL;
        if (n) {
            out->heights = (float *)malloc(n * sizeof(float));
            if (!out->heights) { memset(out, 0, sizeof *out); return 0; }   /* clone OOM -> miss */
            memcpy(out->heights, e->g.heights, n * sizeof(float));
        }
        return 1;
    }
    return 0;
}
static void om_dem_lru_put(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                           const osmmesh_terrain_grid *grid)
{
    if (!grid->heights || grid->rows < 1 || grid->cols < 1 || ctx->dem_cap <= 0) return;
    int vict = -1; uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < ctx->dem_cap; i++) {
        if (!ctx->dem_lru[i].used) { vict = i; break; }
        if (ctx->dem_lru[i].seq < oldest) { oldest = ctx->dem_lru[i].seq; vict = i; }
    }
    size_t n = (size_t)grid->rows * grid->cols;
    float *h = (float *)malloc(n * sizeof(float));
    if (!h) return;                                      /* can't cache -> skip, correctness unaffected */
    memcpy(h, grid->heights, n * sizeof(float));
    om_dem_ent *e = &ctx->dem_lru[vict];
    if (e->used) osmmesh_terrain_grid_free(&e->g);       /* evict */
    e->used = 1; e->z = z; e->x = x; e->y = y;
    e->g.heights = h; e->g.rows = grid->rows; e->g.cols = grid->cols;
    e->seq = ++ctx->dem_seq;
}

/* WITHOUT stitching: splitting raw from stitched is what avoids unbounded recursion, since the
 * stitch pass reads its four neighbours raw. */
static int fetch_terrain_grid_raw(osmmesh_ctx *ctx,
                                  uint8_t z, uint32_t x, uint32_t y,
                                  osmmesh_terrain_grid *out_grid)
{
    memset(out_grid, 0, sizeof *out_grid);
    if (om_dem_lru_get(ctx, z, x, y, out_grid)) return OSMMESH_OK;
    if (!ctx->tile_provider) return OSMMESH_OK;

    /* Past the provider's max zoom: step up to the parent and crop. A tile server has no header
     * stating its max zoom, so the host declares it. */
    uint8_t ter_z = z;
    uint32_t ter_x = x, ter_y = y;
    uint32_t sub_x = 0, sub_y = 0, sub_div = 1;
    int src_maxz = (ctx->provider_terrain_max_zoom > 0) ? ctx->provider_terrain_max_zoom : -1;
    if (src_maxz >= 0 && z > src_maxz) {
        uint8_t dz = (uint8_t)(z - src_maxz);
        if (dz > 16) return OSMMESH_OK; /* absurd gap; give up silently */
        ter_z = (uint8_t)src_maxz;
        sub_div = 1u << dz;
        ter_x = x >> dz;
        ter_y = y >> dz;
        sub_x = x & (sub_div - 1);
        sub_y = y & (sub_div - 1);
    }

    uint8_t *png = NULL; size_t png_len = 0;
    if (!om_tile_bytes(ctx, OSMMESH_TILE_TERRAIN, ter_z, ter_x, ter_y, &png, &png_len) ||
        !png || png_len == 0) {
        free(png);
        return OSMMESH_OK;   /* absent / not yet fetched */
    }

    osmmesh_terrain_grid grid = {};
    int drc = osmmesh_terrain_decode_png(png, png_len, &grid);
    free(png);
    if (drc != OSMMESH_TERRAIN_OK) return OSMMESH_ERR_DECODE;

    if (sub_div > 1) {
        uint32_t crop_cols = grid.cols / sub_div;
        uint32_t crop_rows = grid.rows / sub_div;
        if (crop_cols < 2 || crop_rows < 2) {
            osmmesh_terrain_grid_free(&grid);
            return OSMMESH_OK;
        }
        uint32_t src_x0 = sub_x * crop_cols;
        uint32_t src_y0 = sub_y * crop_rows;
        /* Exactly the sub-tile's own texels, no overlap column: a texel is an AREA, so the sub-tile
         * is covered by crop_cols of them and the value ON its border comes from the stitch, the same
         * way an uncropped tile gets it. An extra column would put a sample half a texel outside. */
        uint32_t out_cols = crop_cols;
        uint32_t out_rows = crop_rows;
        float *crop = (float *)malloc((size_t)out_rows * out_cols * sizeof(float));
        if (!crop) {
            osmmesh_terrain_grid_free(&grid);
            return OSMMESH_ERR_OOM;
        }
        for (uint32_t r = 0; r < out_rows; r++) {
            memcpy(crop + (size_t)r * out_cols,
                   grid.heights + (size_t)(src_y0 + r) * grid.cols + src_x0,
                   (size_t)out_cols * sizeof(float));
        }
        free(grid.heights);
        grid.heights = crop;
        grid.rows = out_rows;
        grid.cols = out_cols;
    }

    om_dem_lru_put(ctx, z, x, y, &grid);   /* cache a clone; `grid` stays ours to hand back */
    *out_grid = grid;
    return OSMMESH_OK;
}

/* SYMMETRIC averaging: both sides land on the same midpoint, so the heights line up exactly at the
 * seam. Sides 0=W, 1=E, 2=N, 3=S. Dimension mismatches (parent-edge cropping shaves a row) are
 * tolerated — only the overlapping range is averaged. */
static int stitch_edge(osmmesh_ctx *ctx, osmmesh_terrain_grid *self,
                       uint8_t z, uint32_t nx, uint32_t ny, int side)
{
    osmmesh_terrain_grid n = {};
    int rc = fetch_terrain_grid_raw(ctx, z, nx, ny, &n);
    if (rc != OSMMESH_OK || !n.heights || n.rows < 2 || n.cols < 2) {
        osmmesh_terrain_grid_free(&n);
        /* A neighbour that is absent or will not decode costs this edge its averaging and nothing
         * more — that tile reports its own decode failure when it is built. A REFUSED ALLOCATION is
         * not the neighbour's problem: it is this whole context's, and it travels. */
        return rc == OSMMESH_ERR_OOM ? rc : OSMMESH_OK;
    }

    if (side == 0 || side == 1) { /* W or E: column average over rows */
        uint32_t self_col = (side == 0) ? 0 : (self->cols - 1);
        uint32_t neig_col = (side == 0) ? (n.cols - 1) : 0;
        uint32_t rows = (self->rows < n.rows) ? self->rows : n.rows;
        for (uint32_t r = 0; r < rows; r++) {
            float a = self->heights[(size_t)r * self->cols + self_col];
            float b = n.heights[(size_t)r * n.cols + neig_col];
            self->heights[(size_t)r * self->cols + self_col] = 0.5f * (a + b);
        }
    } else {                       /* N or S: row average over cols */
        uint32_t self_row = (side == 2) ? 0 : (self->rows - 1);
        uint32_t neig_row = (side == 2) ? (n.rows - 1) : 0;
        uint32_t cols = (self->cols < n.cols) ? self->cols : n.cols;
        for (uint32_t c = 0; c < cols; c++) {
            float a = self->heights[(size_t)self_row * self->cols + c];
            float b = n.heights[(size_t)neig_row * n.cols + c];
            self->heights[(size_t)self_row * self->cols + c] = 0.5f * (a + b);
        }
    }

    osmmesh_terrain_grid_free(&n);
    return OSMMESH_OK;
}

/* The raw fetch plus neighbour-edge averaging on four independent sides. */
static int fetch_terrain_grid(osmmesh_ctx *ctx,
                              uint8_t z, uint32_t x, uint32_t y,
                              osmmesh_terrain_grid *out_grid)
{
    int rc = fetch_terrain_grid_raw(ctx, z, x, y, out_grid);
    if (rc != OSMMESH_OK || !out_grid->heights) return rc;

    /* The map's own width at this rung. Past it there is no tile to average with, and the request
     * would be one a tile server can only answer with an error (tiles/src/route.c bounds x and y) --
     * which the caller must be free to read as a defect. */
    const uint32_t n = 1u << z;
    if (x > 0) {
        rc = stitch_edge(ctx, out_grid, z, x - 1, y, 0);   /* W */
        if (rc != OSMMESH_OK) return rc;
    }
    if (x + 1 < n) {
        rc = stitch_edge(ctx, out_grid, z, x + 1, y, 1);   /* E */
        if (rc != OSMMESH_OK) return rc;
    }
    if (y > 0) {
        rc = stitch_edge(ctx, out_grid, z, x, y - 1, 2);   /* N */
        if (rc != OSMMESH_OK) return rc;
    }
    if (y + 1 < n) return stitch_edge(ctx, out_grid, z, x, y + 1, 3);   /* S */
    return OSMMESH_OK;
}

/* ENU metres, from an already-decoded grid. */
static int build_terrain_mesh_from_grid(osmmesh_ctx *ctx,
                                        const osmmesh_terrain_grid *grid,
                                        const osmmesh_tile_enu_map *map,
                                        osmmesh_mesh **out_mesh)
{
    *out_mesh = NULL;
    if (!grid || !grid->heights || grid->rows < 2 || grid->cols < 2)
        return OSMMESH_OK;

    osmmesh_terrain_build_opts opts;
    opts.stride          = 1;
    if (ctx->has_terrain_opts) opts = ctx->terrain_opts;
    if (opts.stride == 0) opts.stride = 1;

    osmmesh_mesh *m = (osmmesh_mesh *)calloc(1, sizeof(*m));
    if (!m) return OSMMESH_ERR_OOM;

    int brc = osmmesh_terrain_build_mesh(grid, map, &opts, m);
    if (brc != OSMMESH_TERRAIN_OK) {
        free(m);
        return (brc == OSMMESH_TERRAIN_ERR_OOM) ? OSMMESH_ERR_OOM : OSMMESH_ERR_DECODE;
    }
    *out_mesh = m;
    return OSMMESH_OK;
}

int osmmesh_create(const osmmesh_config *cfg, osmmesh_ctx **out)
{
    if (!cfg || !out) return OSMMESH_ERR_ARG;
    *out = NULL;
    if (!cfg->tile_provider) return OSMMESH_ERR_CONFIG;

    osmmesh_ctx *ctx = (osmmesh_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx) return OSMMESH_ERR_OOM;

    if (osmmesh_enu_init(&ctx->enu, cfg->origin_lat, cfg->origin_lon) != OSMMESH_GEO_OK) {
        free(ctx);
        return OSMMESH_ERR_CONFIG;
    }

    if (cfg->terrain_opts) {
        ctx->terrain_opts     = *cfg->terrain_opts;
        ctx->has_terrain_opts = 1;
    }

    ctx->tile_provider             = cfg->tile_provider;
    ctx->tile_provider_user        = cfg->tile_provider_user;
    ctx->provider_terrain_max_zoom = cfg->provider_terrain_max_zoom;
    ctx->enable_terrain            = cfg->enable_terrain ? 1 : 0;
    ctx->dem_cap                   = cfg->dem_cache_tiles > 0 &&
                                     cfg->dem_cache_tiles < OM_DEM_LRU_CAP
                                         ? cfg->dem_cache_tiles : OM_DEM_LRU_CAP;

    *out = ctx;
    return OSMMESH_OK;
}

void osmmesh_destroy(osmmesh_ctx *ctx)
{
    if (!ctx) return;
    for (int i = 0; i < OM_DEM_LRU_CAP; i++)
        if (ctx->dem_lru[i].used) osmmesh_terrain_grid_free(&ctx->dem_lru[i].g);
    osmmesh_terrain_grid_free(&ctx->borrowed);
    free(ctx);
}

static size_t om_grid_bytes(const osmmesh_terrain_grid *g)
{
    if (!g->heights || g->rows < 1 || g->cols < 1) return 0;
    return (size_t)g->rows * (size_t)g->cols * sizeof(float);
}

size_t osmmesh_ctx_heap_bytes(const osmmesh_ctx *ctx)
{
    if (!ctx) return 0;
    size_t bytes = sizeof(*ctx) + om_grid_bytes(&ctx->borrowed);
    for (int i = 0; i < OM_DEM_LRU_CAP; i++)
        if (ctx->dem_lru[i].used) bytes += om_grid_bytes(&ctx->dem_lru[i].g);
    return bytes;
}

int osmmesh_fetch_tile(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                       osmmesh_tile *out)
{
    if (!ctx || !out) return OSMMESH_ERR_ARG;

    memset(out, 0, sizeof *out);
    out->z = z; out->x = x; out->y = y;
    if (!ctx->enable_terrain) return OSMMESH_OK;

    osmmesh_terrain_grid grid = {};
    int trc = fetch_terrain_grid(ctx, z, x, y, &grid);
    if (trc != OSMMESH_OK) {
        osmmesh_terrain_grid_free(&grid);   /* a stitch can fail with the tile's own grid already up */
        return trc;
    }

    if (grid.heights && grid.rows >= 2 && grid.cols >= 2) {
        osmmesh_tile_enu_map map;
        osmmesh_tile_enu_map_init(&map, &ctx->enu, z, x, y, 4096);
        int brc = build_terrain_mesh_from_grid(ctx, &grid, &map, &out->terrain);
        if (brc != OSMMESH_OK) {
            osmmesh_terrain_grid_free(&grid);
            return brc;
        }
    }

    osmmesh_terrain_grid_free(&grid);
    return OSMMESH_OK;
}

int osmmesh_tile_grid(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                      const osmmesh_terrain_grid **out_grid,
                      uint32_t *row_postings, uint32_t *col_postings)
{
    if (!ctx || !out_grid) return OSMMESH_ERR_ARG;
    *out_grid = NULL;
    osmmesh_terrain_grid_free(&ctx->borrowed);
    int rc = fetch_terrain_grid(ctx, z, x, y, &ctx->borrowed);
    if (rc != OSMMESH_OK) {
        osmmesh_terrain_grid_free(&ctx->borrowed);
        return rc;
    }
    if (!ctx->borrowed.heights || ctx->borrowed.rows < 2 || ctx->borrowed.cols < 2) {
        osmmesh_terrain_grid_free(&ctx->borrowed);
        return OSMMESH_OK;
    }
    /* The builder's own stride, read off the context rather than assumed, so the posting lattice the
     * caller lands on is the one the mesh will carry. */
    uint32_t stride = ctx->has_terrain_opts && ctx->terrain_opts.stride ? ctx->terrain_opts.stride : 1;
    if (row_postings) *row_postings = osmmesh_terrain_postings(ctx->borrowed.rows, stride);
    if (col_postings) *col_postings = osmmesh_terrain_postings(ctx->borrowed.cols, stride);
    *out_grid = &ctx->borrowed;
    return OSMMESH_OK;
}

void osmmesh_free_tile(osmmesh_tile *tile)
{
    if (!tile) return;
    if (tile->terrain) {
        osmmesh_mesh_free(tile->terrain);
        free(tile->terrain);
    }
    memset(tile, 0, sizeof *tile);
}
