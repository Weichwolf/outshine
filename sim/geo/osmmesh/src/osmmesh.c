/* libosmmesh/src/osmmesh.c
 *
 * T8 integration layer. Owns the public `osmmesh_ctx`, orchestrates the
 * PMTiles reader + MVT decoder + per-category mesh builders into a single
 * `osmmesh_fetch_tile` entry point, and provides the matching free routine.
 *
 * Dispatch pipeline for one (z,x,y):
 *
 *   1. PMTiles fetch on the vector archive.
 *      - NOT_FOUND  -> empty bundle (OSMMESH_OK, zero counts, no log).
 *      - IO / CORRUPT -> OSMMESH_ERR_IO / _DECODE.
 *   2. MVT decode on the raw payload.
 *      - decode failure -> OSMMESH_ERR_DECODE, bundle stays free.
 *   3. tile_enu_map init from (z,x,y) + cached ENU ctx (once per tile).
 *   4. Iterate layers:
 *        building layers      -> osmmesh_building_build per polygon feature
 *        linear layers        -> osmmesh_linear_build   per linestring feat
 *        anything else        -> ignored
 *      Per-feature SKIP is counted in `n_skipped_features`; ERR_OOM bubbles.
 *   5. Optional terrain:
 *        PMTiles fetch on the terrain archive -> PNG bytes -> heightgrid
 *        -> terrain_build_mesh. NOT_FOUND is legal (terrain==NULL).
 */

#include "osmmesh/osmmesh.h"

#include "building_internal.h"
#include "linear_internal.h"
#include "terrain_planish.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 *  T12 parameters (algorithmic, universal — not config-exposed).
 * ====================================================================== */

static const float OSMMESH_LINEAR_CLEARANCE_M    = 0.05f;
/* Terrain notch depth: every heightgrid pixel under a linear ribbon is
 * sunk by this many metres (cosine falloff beyond half-width). Has to
 * beat the gradient mismatch between the terrain-mesh triangulation and
 * the ribbon-mesh triangulation: both interpolate between their own
 * vertices, so a band crossing a slope can still have the terrain
 * triangle plane poke through even though the nearest heightgrid pixel
 * is technically sunk. 15 cm covers typical Copernicus 30 m mesh slopes;
 * visible as a slight dip at the ribbon edge, not a trench. */
static const float OSMMESH_NOTCH_DEPTH_M         = 0.20f;
/* OSMMESH_BUILDING_CLEARANCE_M == 0 per T12 spec; the max-point of the
 * footprint terrain sample is used verbatim as base_z, so we never emit a
 * literal in this TU. Keeping this comment instead of a const avoids
 * -Wunused under -Werror. */
/* WINDOW_FACTOR and FALLOFF_FACTOR live in terrain_planish.c. */

/* ========================================================================
 *  Context
 * ====================================================================== */

struct osmmesh_ctx {
    osmmesh_pmtiles *vec_pm;   /* vector archive, required */
    osmmesh_pmtiles *ter_pm;   /* terrain archive, optional (NULL if none) */

    osmmesh_enu_ctx  enu;      /* origin frame for all ENU conversions */

    /* Copy of the caller's config (pointers are borrowed on purpose:
     * the config struct lifetime need only span the _create call; we keep
     * shallow copies of the opts pointers for later use during fetch). */
    osmmesh_building_opts      building_opts;
    osmmesh_linear_opts        linear_opts;
    osmmesh_terrain_build_opts terrain_opts;
    int                        has_building_opts;
    int                        has_linear_opts;
    int                        has_terrain_opts;

    /* FlightBox local extension: host-supplied per-tile byte provider (see osmmesh.h). */
    osmmesh_tile_provider tile_provider;
    void                 *tile_provider_user;

    int enable_terrain;
    int enable_buildings;
    int enable_linears;
};

/* ========================================================================
 *  Layer classification
 *
 *  Shortbread 1.0 exposes vector layers with stable names. We route on the
 *  plain layer name only (no attribute sniffing); unknown layers fall into
 *  the "ignored" bucket and their feature count contributes nothing.
 *
 *  Keep these tables flat rather than hash-built: the layer count per tile
 *  is O(10) in Shortbread, a linear scan is cheaper than hashing.
 * ====================================================================== */

/* --- FlightBox local extension ---
 * Single seam for obtaining a tile's raw bytes: the host's provider if one is configured,
 * otherwise the PMTiles archive as before. Both paths hand over a malloc'd buffer that the
 * caller frees with osmmesh_pmtiles_free_tile (which is free()), so call sites are unchanged. */
static int om_tile_bytes(osmmesh_ctx *ctx, osmmesh_tile_kind kind, osmmesh_pmtiles *pm,
                         uint32_t z, uint32_t x, uint32_t y, uint8_t **out, size_t *len)
{
    if (ctx->tile_provider)
        return ctx->tile_provider(ctx->tile_provider_user, kind, z, x, y, out, len)
               ? OSMMESH_PMTILES_OK : OSMMESH_PMTILES_ERR_NOT_FOUND;
    return osmmesh_pmtiles_fetch(pm, z, x, y, out, len);
}

static int name_matches(const char *name, const char *const *set, size_t n)
{
    if (!name) return 0;
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(name, set[i]) == 0) return 1;
    }
    return 0;
}

static int is_building_layer(const char *name)
{
    /* Shortbread uses "building". Other profiles occasionally use the
     * plural. Keep both as documented in the T8 spec. */
    static const char *const names[] = { "building", "buildings" };
    return name_matches(name, names, sizeof(names) / sizeof(names[0]));
}

static int is_linear_layer(const char *name)
{
    /* Shortbread uses "streets" + "streets_polygons" + "highway_links"
     * + "railways" + "water_lines". Our T1 fixture uses "highway"; keep
     * both live. "roads" / "waterway" are generic OMT-ish aliases. */
    static const char *const names[] = {
        "streets", "highway", "roads", "streets_polygons", "railways",
        "water_lines", "waterway"
    };
    return name_matches(name, names, sizeof(names) / sizeof(names[0]));
}

/* ========================================================================
 *  Tile emission helpers — grow-on-demand SoA for the output bundle.
 *
 *  We don't know up front how many building / linear features will pass
 *  the per-feature SKIP filter, so we grow doubling buffers and shrink to
 *  fit at the end. Zero-sized categories leave NULL pointers.
 * ====================================================================== */

typedef struct {
    osmmesh_mesh          *meshes;
    osmmesh_building_info *infos;
    uint32_t               count;
    uint32_t               cap;
} bundle_buildings;

typedef struct {
    osmmesh_mesh        *meshes;
    osmmesh_linear_info *infos;
    uint32_t             count;
    uint32_t             cap;
} bundle_linears;

static int ensure_cap_buildings(bundle_buildings *b)
{
    if (b->count < b->cap) return 0;
    uint32_t new_cap = b->cap ? b->cap * 2 : 8;
    osmmesh_mesh          *nm = (osmmesh_mesh *)
        realloc(b->meshes, (size_t)new_cap * sizeof(*nm));
    osmmesh_building_info *ni = (osmmesh_building_info *)
        realloc(b->infos,  (size_t)new_cap * sizeof(*ni));
    if (!nm || !ni) {
        /* Keep whichever succeeded so the caller's free path is correct;
         * the next ensure_cap call will retry. */
        if (nm) b->meshes = nm;
        if (ni) b->infos  = ni;
        return -1;
    }
    b->meshes = nm;
    b->infos  = ni;
    b->cap    = new_cap;
    return 0;
}

static int ensure_cap_linears(bundle_linears *b)
{
    if (b->count < b->cap) return 0;
    uint32_t new_cap = b->cap ? b->cap * 2 : 8;
    osmmesh_mesh        *nm = (osmmesh_mesh *)
        realloc(b->meshes, (size_t)new_cap * sizeof(*nm));
    osmmesh_linear_info *ni = (osmmesh_linear_info *)
        realloc(b->infos,  (size_t)new_cap * sizeof(*ni));
    if (!nm || !ni) {
        if (nm) b->meshes = nm;
        if (ni) b->infos  = ni;
        return -1;
    }
    b->meshes = nm;
    b->infos  = ni;
    b->cap    = new_cap;
    return 0;
}

static void free_buildings_bundle(bundle_buildings *b)
{
    if (!b) return;
    for (uint32_t i = 0; i < b->count; ++i) osmmesh_mesh_free(&b->meshes[i]);
    free(b->meshes);
    free(b->infos);
    memset(b, 0, sizeof *b);
}

static void free_linears_bundle(bundle_linears *b)
{
    if (!b) return;
    for (uint32_t i = 0; i < b->count; ++i) osmmesh_mesh_free(&b->meshes[i]);
    free(b->meshes);
    free(b->infos);
    memset(b, 0, sizeof *b);
}

/* ========================================================================
 *  Public API: lifecycle
 * ====================================================================== */

int osmmesh_create(const osmmesh_config *cfg, osmmesh_ctx **out)
{
    if (!cfg || !out) return OSMMESH_ERR_ARG;
    *out = NULL;
    /* Vector source: either a file path or an in-memory buffer; at least
     * one must be present. Memory takes precedence when both are set. */
    const int have_vec_mem = (cfg->vector_data  != NULL && cfg->vector_len  > 0);
    const int have_vec_url = (cfg->vector_url   != NULL);
    if (!have_vec_mem && !have_vec_url) return OSMMESH_ERR_CONFIG;

    osmmesh_ctx *ctx = (osmmesh_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx) return OSMMESH_ERR_OOM;

    /* Origin + ENU frame. Guard against the geo.c range check (lat band). */
    if (osmmesh_enu_init(&ctx->enu, cfg->origin_lat, cfg->origin_lon) != OSMMESH_GEO_OK) {
        free(ctx);
        return OSMMESH_ERR_CONFIG;
    }

    /* Open vector archive. Missing data is an I/O error (configuration
     * problem the caller needs to see, unlike a missing-tile). */
    int vrc;
    if (have_vec_mem) {
        vrc = osmmesh_pmtiles_open_memory(&ctx->vec_pm,
                                           cfg->vector_data, cfg->vector_len);
    } else {
        vrc = osmmesh_pmtiles_open_file(&ctx->vec_pm, cfg->vector_url);
    }
    if (vrc != OSMMESH_PMTILES_OK) {
        free(ctx);
        return OSMMESH_ERR_IO;
    }

    const int have_ter_mem = (cfg->terrain_data != NULL && cfg->terrain_len > 0);
    const int have_ter_url = (cfg->terrain_url  != NULL);
    if (have_ter_mem || have_ter_url) {
        int trc;
        if (have_ter_mem) {
            trc = osmmesh_pmtiles_open_memory(&ctx->ter_pm,
                                               cfg->terrain_data, cfg->terrain_len);
        } else {
            trc = osmmesh_pmtiles_open_file(&ctx->ter_pm, cfg->terrain_url);
        }
        if (trc != OSMMESH_PMTILES_OK) {
            osmmesh_pmtiles_close(ctx->vec_pm);
            free(ctx);
            return OSMMESH_ERR_IO;
        }
    }

    if (cfg->building_opts) {
        ctx->building_opts     = *cfg->building_opts;
        ctx->has_building_opts = 1;
    }
    if (cfg->linear_opts) {
        ctx->linear_opts     = *cfg->linear_opts;
        ctx->has_linear_opts = 1;
    }
    if (cfg->terrain_opts) {
        ctx->terrain_opts     = *cfg->terrain_opts;
        ctx->has_terrain_opts = 1;
    }

    ctx->tile_provider      = cfg->tile_provider;
    ctx->tile_provider_user = cfg->tile_provider_user;

    ctx->enable_terrain   = cfg->enable_terrain   ? 1 : 0;
    ctx->enable_buildings = cfg->enable_buildings ? 1 : 0;
    ctx->enable_linears   = cfg->enable_linears   ? 1 : 0;

    *out = ctx;
    return OSMMESH_OK;
}

void osmmesh_destroy(osmmesh_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->vec_pm) osmmesh_pmtiles_close(ctx->vec_pm);
    if (ctx->ter_pm) osmmesh_pmtiles_close(ctx->ter_pm);
    free(ctx);
}

/* ========================================================================
 *  Public API: per-tile fetch
 * ====================================================================== */

/* Skip features whose centroid lies outside [0, extent). MVT spec lets
 * features bleed into neighbouring tiles via a buffer zone for join
 * continuity; without filtering, multi-tile loading would render the same
 * building/street on both sides of the border, and single-tile demos show
 * geometry hanging past the terrain mesh edge. We pick the centroid as
 * the feature's "owning tile" — matches Planetiler/OSM convention. */
static int feature_centroid_in_tile(const osmmesh_mvt_layer *L,
                                     const osmmesh_mvt_feature *f)
{
    if (!f || f->n_coords == 0) return 0;
    double sx = 0.0, sy = 0.0;
    for (size_t i = 0; i < f->n_coords; ++i) {
        sx += (double)f->coords[i].x;
        sy += (double)f->coords[i].y;
    }
    sx /= (double)f->n_coords;
    sy /= (double)f->n_coords;
    double ext = (double)osmmesh_mvt_layer_extent(L);
    return (sx >= 0.0 && sx < ext && sy >= 0.0 && sy < ext);
}

static int build_buildings(osmmesh_ctx *ctx,
                            const osmmesh_mvt_layer *L,
                            const osmmesh_tile_enu_map *map,
                            const osmmesh_terrain_grid *terrain_raw,
                            bundle_buildings *out,
                            uint32_t *n_skipped)
{
    const osmmesh_building_opts *opts = ctx->has_building_opts
                                         ? &ctx->building_opts : NULL;

    size_t nf = osmmesh_mvt_num_features(L);
    for (size_t i = 0; i < nf; ++i) {
        const osmmesh_mvt_feature *f = osmmesh_mvt_feature_at(L, i);
        if (!f) continue;
        if (f->geom_type != OSMMESH_MVT_GEOM_POLYGON) { ++*n_skipped; continue; }
        if (!feature_centroid_in_tile(L, f))           { ++*n_skipped; continue; }

        if (ensure_cap_buildings(out) != 0) return OSMMESH_ERR_OOM;

        osmmesh_mesh          *m = &out->meshes[out->count];
        osmmesh_building_info *info = &out->infos[out->count];
        memset(m,    0, sizeof *m);
        memset(info, 0, sizeof *info);

        /* T12: buildings anchor to RAW (un-planished) terrain — they stand
         * on real ground, not on the planished road plane. */
        int rc = osmmesh_building_build_with_terrain(
                   L, f, map, opts, terrain_raw, m, info);
        if (rc == OSMMESH_BUILDING_OK) {
            ++out->count;
        } else if (rc == OSMMESH_BUILDING_ERR_SKIP) {
            ++*n_skipped;
        } else if (rc == OSMMESH_BUILDING_ERR_OOM) {
            return OSMMESH_ERR_OOM;
        } else {
            /* ERR_ARG: shouldn't happen — we validated above. Treat as skip
             * defensively rather than failing the entire tile. */
            ++*n_skipped;
        }
    }
    return OSMMESH_OK;
}

static int build_linears(osmmesh_ctx *ctx,
                          const osmmesh_mvt_layer *L,
                          const osmmesh_tile_enu_map *map,
                          const osmmesh_terrain_grid *terrain_flat,
                          bundle_linears *out,
                          uint32_t *n_skipped,
                          int require_centroid)
{
    const osmmesh_linear_opts *opts = ctx->has_linear_opts
                                       ? &ctx->linear_opts : NULL;

    size_t nf = osmmesh_mvt_num_features(L);
    for (size_t i = 0; i < nf; ++i) {
        const osmmesh_mvt_feature *f = osmmesh_mvt_feature_at(L, i);
        if (!f) continue;
        if (f->geom_type != OSMMESH_MVT_GEOM_LINESTRING) { ++*n_skipped; continue; }
        if (require_centroid && !feature_centroid_in_tile(L, f)) {
            ++*n_skipped; continue;
        }

        if (ensure_cap_linears(out) != 0) return OSMMESH_ERR_OOM;

        osmmesh_mesh        *m = &out->meshes[out->count];
        osmmesh_linear_info *info = &out->infos[out->count];
        memset(m,    0, sizeof *m);
        memset(info, 0, sizeof *info);

        /* T12: linears drape onto the PLANISHED terrain + clearance. */
        int rc = osmmesh_linear_build_with_terrain(
                   L, f, map, opts,
                   terrain_flat, OSMMESH_LINEAR_CLEARANCE_M,
                   m, info);
        if (rc == OSMMESH_LINEAR_OK) {
            ++out->count;
        } else if (rc == OSMMESH_LINEAR_ERR_SKIP) {
            ++*n_skipped;
        } else if (rc == OSMMESH_LINEAR_ERR_OOM) {
            return OSMMESH_ERR_OOM;
        } else {
            ++*n_skipped;
        }
    }
    return OSMMESH_OK;
}

/* Fetch + decode + crop the terrain tile for (z, x, y) into an owned grid,
 * WITHOUT any neighbour-edge stitching. Used both for the primary fetch
 * (then wrapped by fetch_terrain_grid which adds stitching) and for the
 * stitching pass itself reading the four neighbours. Splitting the two
 * avoids unbounded recursion. */
static int fetch_terrain_grid_raw(osmmesh_ctx *ctx,
                                   uint8_t z, uint32_t x, uint32_t y,
                                   osmmesh_terrain_grid *out_grid)
{
    memset(out_grid, 0, sizeof *out_grid);
    if (!ctx->ter_pm) return OSMMESH_OK;

    /* If request z exceeds terrain archive max_zoom, step up to the parent
     * tile and crop the decoded heightmap to the sub-tile region. */
    uint8_t ter_z = z;
    uint32_t ter_x = x, ter_y = y;
    uint32_t sub_x = 0, sub_y = 0, sub_div = 1;
    const osmmesh_pmtiles_header *th = osmmesh_pmtiles_header_get(ctx->ter_pm);
    if (th && z > th->max_zoom) {
        uint8_t dz = (uint8_t)(z - th->max_zoom);
        if (dz > 16) return OSMMESH_OK; /* absurd gap; give up silently */
        ter_z = th->max_zoom;
        sub_div = 1u << dz;
        ter_x = x >> dz;
        ter_y = y >> dz;
        sub_x = x & (sub_div - 1);
        sub_y = y & (sub_div - 1);
    }

    uint8_t *png = NULL; size_t png_len = 0;
    int rc = om_tile_bytes(ctx, OSMMESH_TILE_TERRAIN, ctx->ter_pm, ter_z, ter_x, ter_y, &png, &png_len);
    if (rc == OSMMESH_PMTILES_ERR_NOT_FOUND) return OSMMESH_OK;
    if (rc != OSMMESH_PMTILES_OK) return OSMMESH_ERR_IO;

    osmmesh_terrain_grid grid = {0};
    int drc = osmmesh_terrain_decode_png(png, png_len, &grid);
    osmmesh_pmtiles_free_tile(png);
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
        uint32_t out_cols = crop_cols + 1;
        uint32_t out_rows = crop_rows + 1;
        if (src_x0 + out_cols > grid.cols) out_cols = grid.cols - src_x0;
        if (src_y0 + out_rows > grid.rows) out_rows = grid.rows - src_y0;
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

    *out_grid = grid;
    return OSMMESH_OK;
}

/* Average our edge column/row with the neighbour tile's matching column/row.
 * Symmetric: when the neighbour later fetches and stitches against us, both
 * sides land on the same midpoint value and the heights line up exactly at
 * the seam. Side codes: 0=W (col 0), 1=E (col cols-1), 2=N (row 0), 3=S
 * (row rows-1). Tolerates dimension mismatches (parent-edge cropping can
 * shave a row/column on one side); only the overlapping range is averaged. */
static void stitch_edge(osmmesh_ctx *ctx, osmmesh_terrain_grid *self,
                         uint8_t z, uint32_t nx, uint32_t ny, int side)
{
    osmmesh_terrain_grid n = {0};
    if (fetch_terrain_grid_raw(ctx, z, nx, ny, &n) != OSMMESH_OK ||
        !n.heights || n.rows < 2 || n.cols < 2) {
        osmmesh_terrain_grid_free(&n);
        return;
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
}

/* Wrap the raw fetch with neighbour-edge averaging. Adds 4 fetches+decodes
 * per tile (each side independent). Cost is acceptable for MVP — the
 * memory and decode are cheap, and the resulting tile is seam-clean for
 * neighbour tiles that apply the same averaging. */
static int fetch_terrain_grid(osmmesh_ctx *ctx,
                               uint8_t z, uint32_t x, uint32_t y,
                               osmmesh_terrain_grid *out_grid)
{
    int rc = fetch_terrain_grid_raw(ctx, z, x, y, out_grid);
    if (rc != OSMMESH_OK || !out_grid->heights) return rc;

    if (x > 0)               stitch_edge(ctx, out_grid, z, x - 1, y, 0); /* W */
    /* East: no upper bound check — pmtiles_fetch returns NOT_FOUND for
     * out-of-archive, stitch_edge then drops the call silently. */
                              stitch_edge(ctx, out_grid, z, x + 1, y, 1); /* E */
    if (y > 0)               stitch_edge(ctx, out_grid, z, x, y - 1, 2); /* N */
                              stitch_edge(ctx, out_grid, z, x, y + 1, 3); /* S */

    return OSMMESH_OK;
}

/* Duplicate a heightgrid (rows*cols floats). Both must be freed by caller. */
static int clone_grid(const osmmesh_terrain_grid *src, osmmesh_terrain_grid *dst)
{
    memset(dst, 0, sizeof *dst);
    if (!src || !src->heights || src->rows < 1 || src->cols < 1) return 0;
    size_t n = (size_t)src->rows * (size_t)src->cols;
    dst->heights = (float *)malloc(n * sizeof(float));
    if (!dst->heights) return -1;
    memcpy(dst->heights, src->heights, n * sizeof(float));
    dst->rows = src->rows;
    dst->cols = src->cols;
    return 0;
}

/* Build a terrain mesh from an already-decoded heightgrid. */
static int build_terrain_mesh_from_grid(osmmesh_ctx *ctx,
                                          const osmmesh_terrain_grid *grid,
                                          const osmmesh_tile_enu_map *map,
                                          osmmesh_mesh **out_mesh)
{
    *out_mesh = NULL;
    if (!grid || !grid->heights || grid->rows < 2 || grid->cols < 2)
        return OSMMESH_OK;

    osmmesh_terrain_build_opts opts = {
        .stride          = 1,
        .compute_normals = 1,
        .add_skirt       = 0,
        .skirt_depth_m   = 0.0f,
    };
    if (ctx->has_terrain_opts) opts = ctx->terrain_opts;
    if (opts.stride == 0) opts.stride = 1;

    osmmesh_mesh *m = (osmmesh_mesh *)calloc(1, sizeof(*m));
    if (!m) return OSMMESH_ERR_OOM;

    int brc = osmmesh_terrain_build_mesh(grid, map, &opts, m);
    if (brc != OSMMESH_TERRAIN_OK) {
        free(m);
        if (brc == OSMMESH_TERRAIN_ERR_OOM) return OSMMESH_ERR_OOM;
        return OSMMESH_ERR_DECODE;
    }
    *out_mesh = m;
    return OSMMESH_OK;
}

/* ------------------------------------------------------------------------
 * T12 pipeline (change log: was MVT->terrain->buildings->linears in T8).
 *
 * New order to support terrain-awareness:
 *
 *   1.  Fetch vector MVT tile (NOT_FOUND -> degenerate path, terrain only).
 *   2.  Decode MVT.
 *   3.  Fetch + decode RAW terrain grid (heights as delivered by the DEM).
 *   4.  Clone -> FLAT grid. Collect every linear feature into a line-set,
 *       sorted by class priority ascending. Rasterise into FLAT (higher
 *       priorities overwrite lower ones).
 *   5.  Build terrain mesh from FLAT.
 *   6.  Build building meshes with RAW-terrain drape (buildings stand on
 *       real ground, not on the planished road plane).
 *   7.  Build linear meshes with FLAT-terrain drape + clearance eps (sit
 *       tightly on the planished road plane, not inside it).
 *
 * RAW is only used internally; FLAT is what leaves as the terrain mesh.
 * Both grids are dropped before osmmesh_fetch_tile returns.
 * ------------------------------------------------------------------------ */

/* Build the full notched heightgrid (raw + stitch + notch) for a
 * neighbour tile, used only to read its shared edge during the N-W
 * master pass. Returns OSMMESH_OK even when the vector/MVT fetch fails
 * (we still return a stitched RAW grid and just skip the notch); out
 * grid heights is NULL if there's no terrain at all for that tile. */
static int build_neighbor_flat_full(osmmesh_ctx *ctx,
                                     uint8_t z, uint32_t nx, uint32_t ny,
                                     osmmesh_terrain_grid *out_flat)
{
    memset(out_flat, 0, sizeof *out_flat);

    osmmesh_terrain_grid nraw = {0};
    int rc = fetch_terrain_grid(ctx, z, nx, ny, &nraw);
    if (rc != OSMMESH_OK) return rc;
    if (!nraw.heights) return OSMMESH_OK;  /* no terrain here */

    if (clone_grid(&nraw, out_flat) != 0) {
        osmmesh_terrain_grid_free(&nraw);
        return OSMMESH_ERR_OOM;
    }

    osmmesh_tile_enu_map nmap;
    osmmesh_tile_enu_map_init(&nmap, &ctx->enu, z, nx, ny, 4096);

    if (!ctx->enable_linears) {
        osmmesh_terrain_grid_free(&nraw);
        return OSMMESH_OK;
    }

    uint8_t *mvt_bytes = NULL; size_t mvt_len = 0;
    if (om_tile_bytes(ctx, OSMMESH_TILE_VECTOR, ctx->vec_pm, z, nx, ny, &mvt_bytes, &mvt_len)
            != OSMMESH_PMTILES_OK) {
        osmmesh_terrain_grid_free(&nraw);
        return OSMMESH_OK;
    }
    osmmesh_mvt_tile *nmvt = NULL;
    int drc = osmmesh_mvt_decode(mvt_bytes, mvt_len, &nmvt);
    osmmesh_pmtiles_free_tile(mvt_bytes);
    if (drc != OSMMESH_MVT_OK) {
        osmmesh_terrain_grid_free(&nraw);
        return OSMMESH_OK;
    }

    bundle_linears bl = {0};
    uint32_t skip = 0;
    size_t nl = osmmesh_mvt_num_layers(nmvt);
    for (size_t i = 0; i < nl; ++i) {
        const osmmesh_mvt_layer *L = osmmesh_mvt_layer_at(nmvt, i);
        if (!L) continue;
        const char *lname = osmmesh_mvt_layer_name(L);
        if (!is_linear_layer(lname)) continue;
        uint32_t extent = osmmesh_mvt_layer_extent(L);
        if (extent == 0) extent = 4096;
        osmmesh_tile_enu_map layer_map;
        osmmesh_tile_enu_map_init(&layer_map, &ctx->enu, z, nx, ny, extent);
        (void)build_linears(ctx, L, &layer_map, &nraw, &bl, &skip,
                             /*require_centroid=*/0);
    }
    if (bl.count > 0) {
        (void)osmmesh_terrain_notch_under_linear_meshes(
                  out_flat, &nmap, bl.meshes, bl.count,
                  OSMMESH_NOTCH_DEPTH_M);
    }
    free_linears_bundle(&bl);
    osmmesh_mvt_free(nmvt);
    osmmesh_terrain_grid_free(&nraw);
    return OSMMESH_OK;
}

int osmmesh_fetch_tile(osmmesh_ctx *ctx, uint8_t z, uint32_t x, uint32_t y,
                        osmmesh_tile *out)
{
    if (!ctx || !out) return OSMMESH_ERR_ARG;

    memset(out, 0, sizeof *out);
    out->z = z; out->x = x; out->y = y;

    /* ---- 1+2. Vector fetch + decode --------------------------------- */

    uint8_t *mvt_bytes = NULL; size_t mvt_len = 0;
    int vrc = om_tile_bytes(ctx, OSMMESH_TILE_VECTOR, ctx->vec_pm, z, x, y, &mvt_bytes, &mvt_len);

    osmmesh_mvt_tile *mvt = NULL;
    int have_mvt = 0;
    if (vrc == OSMMESH_PMTILES_OK) {
        if (osmmesh_mvt_decode(mvt_bytes, mvt_len, &mvt) != OSMMESH_MVT_OK) {
            osmmesh_pmtiles_free_tile(mvt_bytes);
            return OSMMESH_ERR_DECODE;
        }
        osmmesh_pmtiles_free_tile(mvt_bytes);
        have_mvt = 1;
    } else if (vrc == OSMMESH_PMTILES_ERR_NOT_FOUND) {
        /* Keep going: terrain may still exist for this tile. */
    } else {
        return OSMMESH_ERR_IO;
    }

    /* ---- 3. RAW terrain --------------------------------------------- */

    osmmesh_terrain_grid grid_raw  = {0};
    osmmesh_terrain_grid grid_flat = {0};
    if (ctx->enable_terrain) {
        int trc = fetch_terrain_grid(ctx, z, x, y, &grid_raw);
        if (trc != OSMMESH_OK) {
            if (have_mvt) osmmesh_mvt_free(mvt);
            return trc;
        }
    }

    /* Shortcut: an ENU map per-extent. For the default 4096 extent it
     * covers the terrain pipeline. Layers re-init this per their own
     * extent when running the building / linear builders. */
    osmmesh_tile_enu_map default_map;
    osmmesh_tile_enu_map_init(&default_map, &ctx->enu, z, x, y, 4096);

    /* ---- 4. Heightgrid for terrain + linears ----
     *
     * T14.8 pipeline: build linear meshes FIRST (they drape on the raw
     * heightgrid + clearance, possibly with bridge ramps from the per-
     * class gradient limit), then notch grid_flat from those linear
     * meshes triangle-by-triangle. The terrain mesh is built last from
     * the notched grid_flat. This makes the test_086 invariant ("every
     * linear triangle is above the terrain mesh") true by construction.
     *
     * Buildings sample grid_raw (real ground), independent of notching.
     */

    int have_grid = (grid_raw.heights && grid_raw.rows >= 2 && grid_raw.cols >= 2);
    if (have_grid) {
        if (clone_grid(&grid_raw, &grid_flat) != 0) {
            osmmesh_terrain_grid_free(&grid_raw);
            if (have_mvt) osmmesh_mvt_free(mvt);
            return OSMMESH_ERR_OOM;
        }
    }

    /* ---- 5. Build building + linear meshes BEFORE the terrain mesh.
     * Linears drape on grid_raw; their final z values are needed for
     * the heightgrid notch. */

    bundle_buildings bb = {0};
    bundle_linears   bl = {0};
    uint32_t n_skipped = 0;

    if (have_mvt) {
        size_t nl = osmmesh_mvt_num_layers(mvt);
        for (size_t i = 0; i < nl; ++i) {
            const osmmesh_mvt_layer *L = osmmesh_mvt_layer_at(mvt, i);
            if (!L) continue;

            const char *lname = osmmesh_mvt_layer_name(L);
            uint32_t    extent = osmmesh_mvt_layer_extent(L);
            if (extent == 0) extent = 4096;

            osmmesh_tile_enu_map map;
            osmmesh_tile_enu_map_init(&map, &ctx->enu, z, x, y, extent);

            if (ctx->enable_buildings && is_building_layer(lname)) {
                int rc = build_buildings(ctx, L, &map,
                                           have_grid ? &grid_raw : NULL,
                                           &bb, &n_skipped);
                if (rc != OSMMESH_OK) {
                    free_buildings_bundle(&bb);
                    free_linears_bundle(&bl);
                    osmmesh_terrain_grid_free(&grid_raw);
                    osmmesh_terrain_grid_free(&grid_flat);
                    osmmesh_mvt_free(mvt);
                    osmmesh_free_tile(out);
                    return rc;
                }
            } else if (ctx->enable_linears && is_linear_layer(lname)) {
                int rc = build_linears(ctx, L, &map,
                                         have_grid ? &grid_raw : NULL,
                                         &bl, &n_skipped,
                                         /*require_centroid=*/1);
                if (rc != OSMMESH_OK) {
                    free_buildings_bundle(&bb);
                    free_linears_bundle(&bl);
                    osmmesh_terrain_grid_free(&grid_raw);
                    osmmesh_terrain_grid_free(&grid_flat);
                    osmmesh_mvt_free(mvt);
                    osmmesh_free_tile(out);
                    return rc;
                }
            } else {
                n_skipped += (uint32_t)osmmesh_mvt_num_features(L);
            }
        }
    }

    /* ---- 6. Notch grid_flat under ALL linear meshes (incl. buffer-zone
     *         features). bl only holds centroid-owned features to avoid
     *         double-rendering in the output; the notch needs EVERY
     *         feature visible in this tile (including buffer duplicates
     *         from neighbours) so adjacent tiles notch their shared edge
     *         pixels identically. Otherwise the terrain cracks open at
     *         tile borders whenever only one neighbour sees a ribbon.
     *
     *         We re-walk the MVT and build a throwaway bundle with
     *         require_centroid=0, notch from it, free. Cost is one
     *         extra linear mesh pass per tile (~1 ms). */
    bundle_linears bl_notch = {0};
    uint32_t notch_skipped = 0;
    if (have_grid && have_mvt && ctx->enable_linears) {
        size_t nl2 = osmmesh_mvt_num_layers(mvt);
        for (size_t i = 0; i < nl2; ++i) {
            const osmmesh_mvt_layer *L = osmmesh_mvt_layer_at(mvt, i);
            if (!L) continue;
            const char *lname = osmmesh_mvt_layer_name(L);
            if (!is_linear_layer(lname)) continue;
            uint32_t extent = osmmesh_mvt_layer_extent(L);
            if (extent == 0) extent = 4096;
            osmmesh_tile_enu_map layer_map;
            osmmesh_tile_enu_map_init(&layer_map, &ctx->enu, z, x, y, extent);
            (void)build_linears(ctx, L, &layer_map, &grid_raw,
                                 &bl_notch, &notch_skipped,
                                 /*require_centroid=*/0);
        }
        if (bl_notch.count > 0) {
            (void)osmmesh_terrain_notch_under_linear_meshes(
                      &grid_flat, &default_map,
                      bl_notch.meshes, bl_notch.count,
                      OSMMESH_NOTCH_DEPTH_M);
        }
        free_linears_bundle(&bl_notch);
    }

    /* ---- 6b. N-W master convention for heightgrid seams ----
     *
     * Even with identical raw + identical notch logic, floating-point
     * drift between neighbour tiles can leave a sub-mm diff at the
     * shared edge. Instead of averaging (which makes every boundary a
     * two-way blend) we use the convention "upper-left tile is master":
     * our West column is taken from the W-neighbour's East column, our
     * North row from the N-neighbour's South row. E and S edges stay
     * as we computed them -- they will in turn be overwritten by our
     * E-neighbour / S-neighbour when they build themselves, so every
     * shared edge has exactly one master and both sides end up bit-
     * identical. Cost: two extra full heightgrid builds per tile
     * (fetch + stitch + notch), ~40 ms each on Hameln. */
    if (have_grid && grid_flat.heights) {
        osmmesh_terrain_grid wflat = {0};
        if (x > 0 &&
            build_neighbor_flat_full(ctx, z, x - 1, y, &wflat) == OSMMESH_OK &&
            wflat.heights && wflat.rows >= grid_flat.rows) {
            uint32_t src_col = wflat.cols - 1;
            uint32_t rows = grid_flat.rows;
            for (uint32_t r = 0; r < rows; ++r) {
                grid_flat.heights[(size_t)r * grid_flat.cols + 0] =
                    wflat.heights[(size_t)r * wflat.cols + src_col];
            }
        }
        osmmesh_terrain_grid_free(&wflat);

        osmmesh_terrain_grid nflat = {0};
        if (y > 0 &&
            build_neighbor_flat_full(ctx, z, x, y - 1, &nflat) == OSMMESH_OK &&
            nflat.heights && nflat.cols >= grid_flat.cols) {
            uint32_t src_row = nflat.rows - 1;
            uint32_t cols = grid_flat.cols;
            for (uint32_t c = 0; c < cols; ++c) {
                grid_flat.heights[(size_t)0 * grid_flat.cols + c] =
                    nflat.heights[(size_t)src_row * nflat.cols + c];
            }
        }
        osmmesh_terrain_grid_free(&nflat);
    }

    /* ---- 7. Build terrain mesh from notched grid_flat ---- */

    if (ctx->enable_terrain && have_grid) {
        int trc = build_terrain_mesh_from_grid(ctx, &grid_flat, &default_map,
                                                 &out->terrain);
        if (trc != OSMMESH_OK) {
            free_buildings_bundle(&bb);
            free_linears_bundle(&bl);
            osmmesh_terrain_grid_free(&grid_raw);
            osmmesh_terrain_grid_free(&grid_flat);
            if (have_mvt) osmmesh_mvt_free(mvt);
            osmmesh_free_tile(out);
            return trc;
        }
    }

    osmmesh_terrain_grid_free(&grid_raw);
    osmmesh_terrain_grid_free(&grid_flat);
    if (have_mvt) osmmesh_mvt_free(mvt);

    /* Shrink bundles; if empty, free the allocation entirely so the
     * output has NULL pointers matching n_buildings/n_linears == 0. */
    if (bb.count == 0) {
        free(bb.meshes); free(bb.infos);
        bb.meshes = NULL; bb.infos = NULL; bb.cap = 0;
    }
    if (bl.count == 0) {
        free(bl.meshes); free(bl.infos);
        bl.meshes = NULL; bl.infos = NULL; bl.cap = 0;
    }

    out->buildings      = bb.meshes;
    out->building_infos = bb.infos;
    out->n_buildings    = bb.count;
    out->linears        = bl.meshes;
    out->linear_infos   = bl.infos;
    out->n_linears      = bl.count;
    out->n_skipped_features = n_skipped;

    return OSMMESH_OK;
}

/* ========================================================================
 *  Public API: tile teardown
 *
 *  Safe on zero-init, safe on double-free. Mirrors osmmesh_mesh_free's
 *  "zero after release" contract so higher-level callers can reuse the
 *  struct slot.
 * ====================================================================== */

void osmmesh_free_tile(osmmesh_tile *tile)
{
    if (!tile) return;

    if (tile->terrain) {
        osmmesh_mesh_free(tile->terrain);
        free(tile->terrain);
    }

    if (tile->buildings) {
        for (uint32_t i = 0; i < tile->n_buildings; ++i) {
            osmmesh_mesh_free(&tile->buildings[i]);
        }
        free(tile->buildings);
    }
    free(tile->building_infos);

    if (tile->linears) {
        for (uint32_t i = 0; i < tile->n_linears; ++i) {
            osmmesh_mesh_free(&tile->linears[i]);
        }
        free(tile->linears);
    }
    free(tile->linear_infos);

    memset(tile, 0, sizeof *tile);
}

/* ========================================================================
 *  T10 JS-binding helpers
 *
 *  Emscripten-exported accessors that let a JS wrapper look inside the
 *  opaque osmmesh_mesh / osmmesh_tile structs without needing to know the
 *  C layout (so we can grow them later without breaking the JS ABI).
 *
 *  Pointer returns are suitable for building zero-copy TypedArray views:
 *     new Float32Array(Module.HEAPF32.buffer, ptr >> 2, 3 * n_vertices)
 *  Lifetime: valid until osmmesh_free_tile releases the owning tile.
 * ====================================================================== */

uint32_t osmmesh_mesh_n_vertices_get(const osmmesh_mesh *m) {
    return m ? m->n_vertices : 0;
}
uint32_t osmmesh_mesh_n_triangles_get(const osmmesh_mesh *m) {
    return m ? m->n_triangles : 0;
}
float *osmmesh_mesh_positions_get(const osmmesh_mesh *m) {
    return m ? m->positions : NULL;
}
float *osmmesh_mesh_normals_get(const osmmesh_mesh *m) {
    return m ? m->normals : NULL;
}
float *osmmesh_mesh_uvs_get(const osmmesh_mesh *m) {
    return m ? m->uvs : NULL;
}
uint32_t *osmmesh_mesh_indices_get(const osmmesh_mesh *m) {
    return m ? m->indices : NULL;
}

/* Tile accessors — JS holds an osmmesh_tile* malloc'd on the heap and
 * reads counts / sub-mesh-pointers through these. */
osmmesh_mesh *osmmesh_tile_terrain_get(const osmmesh_tile *t) {
    return t ? t->terrain : NULL;
}
uint32_t osmmesh_tile_n_buildings_get(const osmmesh_tile *t) {
    return t ? t->n_buildings : 0;
}
uint32_t osmmesh_tile_n_linears_get(const osmmesh_tile *t) {
    return t ? t->n_linears : 0;
}
uint32_t osmmesh_tile_n_skipped_get(const osmmesh_tile *t) {
    return t ? t->n_skipped_features : 0;
}
/* Return the i-th building/linear mesh pointer; NULL on out-of-range. The
 * parallel info arrays are kept off-limits from JS for now — T11 viewer
 * does not need them. */
osmmesh_mesh *osmmesh_tile_building_at(const osmmesh_tile *t, uint32_t i) {
    if (!t || i >= t->n_buildings || !t->buildings) return NULL;
    return &t->buildings[i];
}
osmmesh_mesh *osmmesh_tile_linear_at(const osmmesh_tile *t, uint32_t i) {
    if (!t || i >= t->n_linears || !t->linears) return NULL;
    return &t->linears[i];
}
int osmmesh_tile_linear_kind_at(const osmmesh_tile *t, uint32_t i) {
    if (!t || i >= t->n_linears || !t->linear_infos) return -1;
    return (int)t->linear_infos[i].kind;
}

/* Heap-allocating counterpart to the stack-based osmmesh_tile. JS can't
 * stack-allocate a C struct through cwrap, so it calls this, then passes
 * the pointer back to osmmesh_fetch_tile / osmmesh_free_tile, then frees
 * the wrapper itself via osmmesh_tile_free_wrap. */
osmmesh_tile *osmmesh_tile_alloc(void) {
    osmmesh_tile *t = (osmmesh_tile *)calloc(1, sizeof(osmmesh_tile));
    return t;
}
void osmmesh_tile_free_wrap(osmmesh_tile *t) {
    if (!t) return;
    osmmesh_free_tile(t);
    free(t);
}
