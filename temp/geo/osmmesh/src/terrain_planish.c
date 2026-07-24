/* libosmmesh/src/terrain_planish.c
 *
 * T12: heightmap planishing and terrain-aware sampling. See
 * terrain_planish.h for the interface contract and osmmesh.c for where
 * this module slots into the per-tile pipeline.
 *
 * ========================================================================
 *  Coordinate conventions (cross-checked with terrain.c)
 * ========================================================================
 *
 *  terrain.c places heightgrid pixel (r, c) at:
 *
 *      e = origin_e + c * scale_e * extent / (cols - 1)
 *      n = origin_n + r * scale_n * extent / (rows - 1)
 *
 *  with scale_n < 0 so that row 0 is the NORTH edge, row (rows-1) is SOUTH.
 *  Per-pixel deltas:
 *      dx_e = scale_e * extent / (cols - 1)    (meters per column east)
 *      dy_n = scale_n * extent / (rows - 1)    (meters per row north; <0)
 *
 *  The meters-per-pixel spacing used for window sizing is the unweighted
 *  average of |dx_e| and |dy_n|; for a square tile they are equal to within
 *  the scale_e/scale_n ratio (negligible at our latitudes).
 *
 * ========================================================================
 *  Algorithm overview
 * ========================================================================
 *
 *  Per polyline:
 *    1. Project spine to ENU. Accumulate cumulative arc length per vertex.
 *    2. Densely sample a 1-D height profile along the spine at a step
 *       roughly equal to meters_per_pixel, by bilinear-sampling the RAW
 *       heightgrid. The profile length = n_profile samples with t spacing
 *       total_length / (n_profile - 1).
 *    3. Moving-median smooth the profile with a window of
 *         W = OSMMESH_PLANISH_WINDOW_FACTOR * width_m / meters_per_pixel
 *       samples (clamped to the profile length). Median is robust to hard
 *       terrain discontinuities at hillsides.
 *    4. For each pixel in the rasterised corridor:
 *         - project to the nearest spine segment, obtain (t, dist)
 *         - look up the smoothed height at t (linearly interpolated from
 *           the discrete profile)
 *         - if dist < half_width               : write smoothed_z
 *         - if half_width < dist < F*half_width : cosine blend with RAW_z
 *         - else                                : leave alone
 *    5. Overlap arbitration: a uint8 priority-scratch tracks the class that
 *       last wrote each pixel. A lower-priority write is dropped. Features
 *       are pre-sorted ascending by priority so the high-priority pass is
 *       last and wins.
 */

#include "terrain_planish.h"

/* Need M_PI in strict C99 — _USE_MATH_DEFINES for MSVC-ish libm, the define
 * below for MSYS/glibc (where math.h gates it on _POSIX_C_SOURCE). */
#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 *  Parameters (universal, not config-exposed).
 * ====================================================================== */

static const float OSMMESH_PLANISH_WINDOW_FACTOR  = 3.0f;
static const float OSMMESH_PLANISH_FALLOFF_FACTOR = 2.0f;
/* clearance constants live in osmmesh.c where they are applied. */

/* ========================================================================
 *  Grid <-> ENU conversion helpers
 * ====================================================================== */

static void grid_pixel_span(const osmmesh_terrain_grid *grid,
                             const osmmesh_tile_enu_map *map,
                             double *dx_e, double *dy_n)
{
    *dx_e = map->scale_e * (double)map->extent / (double)(grid->cols - 1);
    *dy_n = map->scale_n * (double)map->extent / (double)(grid->rows - 1);
}

void osmmesh_planish_grid_cell_size(const osmmesh_terrain_grid *grid,
                                     const osmmesh_tile_enu_map *map,
                                     float *out_dx_m, float *out_dy_m)
{
    if (!grid || !map || !out_dx_m || !out_dy_m) return;
    double dx_e, dy_n;
    grid_pixel_span(grid, map, &dx_e, &dy_n);
    *out_dx_m = (float)fabs(dx_e);
    *out_dy_m = (float)fabs(dy_n);
}

/* Bilinear sample at ENU (e, n). Clamps outside the grid to the nearest
 * edge (a tile-edge-straddling feature sees a flat skirt — fine for MVP). */
float osmmesh_planish_sample_bilinear(const osmmesh_terrain_grid *grid,
                                       const osmmesh_tile_enu_map *map,
                                       float e, float n)
{
    if (!grid || !grid->heights || !map) return 0.0f;
    if (grid->rows < 2 || grid->cols < 2) return 0.0f;

    double dx_e, dy_n;
    grid_pixel_span(grid, map, &dx_e, &dy_n);
    if (dx_e == 0.0 || dy_n == 0.0) return 0.0f;

    double col_f = ((double)e - map->origin_e) / dx_e;
    double row_f = ((double)n - map->origin_n) / dy_n;

    /* Clamp to [0, rows-1] x [0, cols-1]. */
    if (col_f < 0.0)                 col_f = 0.0;
    if (col_f > (double)(grid->cols - 1)) col_f = (double)(grid->cols - 1);
    if (row_f < 0.0)                 row_f = 0.0;
    if (row_f > (double)(grid->rows - 1)) row_f = (double)(grid->rows - 1);

    uint32_t c0 = (uint32_t)col_f;
    uint32_t r0 = (uint32_t)row_f;
    uint32_t c1 = (c0 + 1 < grid->cols) ? c0 + 1 : c0;
    uint32_t r1 = (r0 + 1 < grid->rows) ? r0 + 1 : r0;

    double fc = col_f - (double)c0;
    double fr = row_f - (double)r0;

    float h00 = grid->heights[(size_t)r0 * grid->cols + c0];
    float h01 = grid->heights[(size_t)r0 * grid->cols + c1];
    float h10 = grid->heights[(size_t)r1 * grid->cols + c0];
    float h11 = grid->heights[(size_t)r1 * grid->cols + c1];

    double top = (double)h00 * (1.0 - fc) + (double)h01 * fc;
    double bot = (double)h10 * (1.0 - fc) + (double)h11 * fc;
    double h   = top * (1.0 - fr) + bot * fr;
    return (float)h;
}

/* ========================================================================
 *  Line-set container
 * ====================================================================== */

void osmmesh_planish_line_set_init(osmmesh_planish_line_set *s)
{
    if (!s) return;
    s->items = NULL;
    s->count = 0;
    s->cap   = 0;
}

void osmmesh_planish_line_set_free(osmmesh_planish_line_set *s)
{
    if (!s) return;
    for (uint32_t i = 0; i < s->count; ++i) {
        free(s->items[i].xy);
    }
    free(s->items);
    s->items = NULL;
    s->count = 0;
    s->cap   = 0;
}

int osmmesh_planish_line_set_append(osmmesh_planish_line_set *s,
                                      const osmmesh_planish_line *line)
{
    if (!s || !line) return -1;
    if (s->count >= s->cap) {
        uint32_t new_cap = s->cap ? s->cap * 2 : 16;
        osmmesh_planish_line *nb = (osmmesh_planish_line *)
            realloc(s->items, (size_t)new_cap * sizeof(*nb));
        if (!nb) return -1;
        s->items = nb;
        s->cap   = new_cap;
    }
    s->items[s->count++] = *line;
    return 0;
}

/* ========================================================================
 *  Linear classification / width mirror (keeps this module self-contained).
 *  We re-use the public classify + default-width to avoid tag-parsing here.
 * ====================================================================== */

/* Bring in the SAME skip rules as linear.c so the planisher and the mesh
 * builder process an identical set of features. If the mesh builder would
 * skip a feature for short length / unknown class, we must skip it here
 * too — otherwise we'd planish terrain for a road that never gets drawn. */
int osmmesh_planish_collect_layer(const osmmesh_mvt_layer    *layer,
                                    const osmmesh_tile_enu_map *map,
                                    const osmmesh_linear_opts  *opts,
                                    osmmesh_planish_line_set   *out)
{
    if (!layer || !map || !out) return OSMMESH_LINEAR_ERR_ARG;

    float min_length = (opts && opts->min_length_m > 0.0f) ? opts->min_length_m : 0.0f;

    size_t nf = osmmesh_mvt_num_features(layer);
    for (size_t fi = 0; fi < nf; ++fi) {
        const osmmesh_mvt_feature *f = osmmesh_mvt_feature_at(layer, fi);
        if (!f) continue;
        if (f->geom_type != OSMMESH_MVT_GEOM_LINESTRING) continue;
        if (f->n_coords < 2) continue;

        osmmesh_linear_kind kind = osmmesh_linear_classify(layer, f);
        float width = 0.0f;
        if (opts && opts->width_override_m > 0.0f) {
            width = opts->width_override_m;
        } else {
            if (kind == OSMMESH_LINEAR_UNKNOWN) continue;
            width = osmmesh_linear_default_width(kind);
        }
        if (width < 1e-3f) continue;

        int prio = osmmesh_linear_class_priority(kind);
        if (prio < 0) continue;   /* subway / underground: skip planishing */

        uint32_t n_sub = (uint32_t)(f->n_rings ? f->n_rings : 1);
        const uint32_t *ring_offsets = f->ring_offsets;
        uint32_t fallback[2] = { 0, (uint32_t)f->n_coords };
        if (!ring_offsets) ring_offsets = fallback;

        for (uint32_t s = 0; s < n_sub; ++s) {
            uint32_t a = ring_offsets[s];
            uint32_t b = ring_offsets[s + 1];
            if (b <= a || (b - a) < 2) continue;

            uint32_t n_spine = b - a;
            float *xy = (float *)malloc((size_t)n_spine * 2 * sizeof(float));
            if (!xy) return OSMMESH_LINEAR_ERR_OOM;

            float total_len = 0.0f;
            float px = 0.0f, py = 0.0f;
            for (uint32_t i = 0; i < n_spine; ++i) {
                osmmesh_enu p = osmmesh_tile_enu_map_apply(map,
                                                            f->coords[a + i].x,
                                                            f->coords[a + i].y);
                xy[i * 2 + 0] = (float)p.e;
                xy[i * 2 + 1] = (float)p.n;
                if (i > 0) {
                    float ex = xy[i*2+0] - px;
                    float ey = xy[i*2+1] - py;
                    total_len += sqrtf(ex*ex + ey*ey);
                }
                px = xy[i*2+0];
                py = xy[i*2+1];
            }

            if (total_len < 1e-6f || (min_length > 0.0f && total_len < min_length)) {
                free(xy);
                continue;
            }

            osmmesh_planish_line line = {
                .xy       = xy,
                .n        = n_spine,
                .width_m  = width,
                .kind     = kind,
                .priority = prio,
            };
            if (osmmesh_planish_line_set_append(out, &line) != 0) {
                free(xy);
                return OSMMESH_LINEAR_ERR_OOM;
            }
        }
    }
    return OSMMESH_LINEAR_OK;
}

/* ========================================================================
 *  Moving-median smoothing
 *
 *  For each sample i in [0..N), collect all values within window/2 around
 *  it (clamped to array bounds), sort, take the median. O(N*W log W) which
 *  is fine for our N ~ a few hundred, W ~ tens.
 * ====================================================================== */

static int float_cmp(const void *a, const void *b)
{
    float fa = *(const float *)a, fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return  1;
    return 0;
}

static void moving_median(const float *in, uint32_t n, uint32_t window,
                            float *out)
{
    if (window < 1) window = 1;
    if (window > n) window = n;

    /* Scratch buffer, stack-allocated up to a ceiling; heap beyond. */
    float stack_buf[256];
    float *buf = stack_buf;
    float *heap_buf = NULL;
    if (window > sizeof(stack_buf) / sizeof(stack_buf[0])) {
        heap_buf = (float *)malloc((size_t)window * sizeof(float));
        if (!heap_buf) {
            /* Degrade to a copy — better than corrupting output. */
            memcpy(out, in, (size_t)n * sizeof(float));
            return;
        }
        buf = heap_buf;
    }

    uint32_t half = window / 2;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t lo = (i > half) ? (i - half) : 0;
        uint32_t hi = (i + half + 1 < n) ? (i + half + 1) : n;
        uint32_t m  = hi - lo;
        memcpy(buf, in + lo, (size_t)m * sizeof(float));
        qsort(buf, m, sizeof(float), float_cmp);
        if (m & 1) out[i] = buf[m / 2];
        else       out[i] = 0.5f * (buf[m / 2 - 1] + buf[m / 2]);
    }

    free(heap_buf);
}

/* ========================================================================
 *  Per-polyline rasterisation
 * ====================================================================== */

typedef struct {
    osmmesh_terrain_grid       *grid;
    const osmmesh_tile_enu_map *map;
    uint8_t                    *prio_scratch;   /* rows*cols, 0xFF = no write yet */
    /* Derived for speed. */
    double origin_e, origin_n;
    double dx_e, dy_n;            /* grid pixel step in ENU (meters) */
    double inv_dx_e, inv_dy_n;
    float  meters_per_pixel;
} planish_ctx;

static void planish_ctx_init(planish_ctx *ctx,
                              osmmesh_terrain_grid *grid,
                              const osmmesh_tile_enu_map *map,
                              uint8_t *prio_scratch)
{
    ctx->grid = grid;
    ctx->map  = map;
    ctx->prio_scratch = prio_scratch;
    double dx_e, dy_n;
    grid_pixel_span(grid, map, &dx_e, &dy_n);
    ctx->origin_e = map->origin_e;
    ctx->origin_n = map->origin_n;
    ctx->dx_e = dx_e;
    ctx->dy_n = dy_n;
    ctx->inv_dx_e = (dx_e != 0.0) ? 1.0 / dx_e : 0.0;
    ctx->inv_dy_n = (dy_n != 0.0) ? 1.0 / dy_n : 0.0;
    ctx->meters_per_pixel = (float)(0.5 * (fabs(dx_e) + fabs(dy_n)));
}

/* Convert ENU (e, n) -> continuous (col, row). */
static void enu_to_grid(const planish_ctx *ctx, float e, float n,
                          double *col, double *row)
{
    *col = ((double)e - ctx->origin_e) * ctx->inv_dx_e;
    *row = ((double)n - ctx->origin_n) * ctx->inv_dy_n;
}

/* Convert (row, col) grid indices -> ENU. */
static void grid_to_enu(const planish_ctx *ctx, uint32_t r, uint32_t c,
                         float *e, float *n)
{
    *e = (float)(ctx->origin_e + (double)c * ctx->dx_e);
    *n = (float)(ctx->origin_n + (double)r * ctx->dy_n);
}

/* Compute smoothed profile for one polyline.
 *
 * Returns the number of samples, writes into caller-owned profile_z + step.
 * profile_step = total_length / (n_profile - 1) (meters per sample).
 * profile_total_length is the arc length sum.
 *
 * n_profile is clamped to [2, max_profile]. max_profile bounds memory; we
 * use ceil(total_length / meters_per_pixel) + 1 which is O(tile_diagonal /
 * pixel_size) ~ 500 at z=14.
 */
static int build_smoothed_profile(const planish_ctx *ctx,
                                    const osmmesh_planish_line *line,
                                    float **out_profile,
                                    uint32_t *out_n_profile,
                                    float  *out_step,
                                    float  *out_total_len,
                                    /* per-spine-vertex cumulative length: */
                                    float **out_spine_t)
{
    /* Cumulative lengths per spine vertex. */
    float *spine_t = (float *)malloc((size_t)line->n * sizeof(float));
    if (!spine_t) return -1;
    spine_t[0] = 0.0f;
    float total = 0.0f;
    for (uint32_t i = 1; i < line->n; ++i) {
        float ex = line->xy[i*2+0] - line->xy[(i-1)*2+0];
        float ey = line->xy[i*2+1] - line->xy[(i-1)*2+1];
        total += sqrtf(ex*ex + ey*ey);
        spine_t[i] = total;
    }
    if (total < 1e-6f) { free(spine_t); return -1; }

    float mpp = ctx->meters_per_pixel;
    if (mpp < 1e-3f) mpp = 1.0f;
    uint32_t n_profile = (uint32_t)ceilf(total / mpp) + 1u;
    if (n_profile < 4) n_profile = 4;
    if (n_profile > 4096) n_profile = 4096;   /* safety clamp */

    float *raw = (float *)malloc((size_t)n_profile * sizeof(float));
    float *sm  = (float *)malloc((size_t)n_profile * sizeof(float));
    if (!raw || !sm) { free(raw); free(sm); free(spine_t); return -1; }

    float step = total / (float)(n_profile - 1);

    /* Walk the spine, for each profile sample compute its ENU (e, n) and
     * sample the raw heightfield. Two-pointer advance through spine_t. */
    uint32_t seg = 0;
    for (uint32_t i = 0; i < n_profile; ++i) {
        float t = (float)i * step;
        if (t > total) t = total;
        while (seg + 1 < line->n - 1 && spine_t[seg + 1] < t) ++seg;
        float t0 = spine_t[seg];
        float t1 = spine_t[seg + 1];
        float L  = (t1 > t0) ? (t1 - t0) : 1.0f;
        float f  = (t - t0) / L;
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        float ex = line->xy[seg * 2 + 0] * (1.0f - f) + line->xy[(seg+1)*2 + 0] * f;
        float en = line->xy[seg * 2 + 1] * (1.0f - f) + line->xy[(seg+1)*2 + 1] * f;
        raw[i] = osmmesh_planish_sample_bilinear(ctx->grid, ctx->map, ex, en);
    }

    /* Moving-median window in samples. */
    uint32_t window = (uint32_t)ceilf(
        OSMMESH_PLANISH_WINDOW_FACTOR * line->width_m / mpp);
    if (window < 3) window = 3;
    if ((window & 1) == 0) window += 1;   /* prefer odd */
    if (window > n_profile) window = n_profile;

    moving_median(raw, n_profile, window, sm);

    free(raw);

    *out_profile        = sm;
    *out_n_profile      = n_profile;
    *out_step           = step;
    *out_total_len      = total;
    *out_spine_t        = spine_t;
    return 0;
}

/* Sample the smoothed profile at arc length t (meters). Linear-interp
 * between two discrete samples. Out-of-range t clamps to endpoints. */
static float sample_profile(const float *profile, uint32_t n,
                              float step, float t)
{
    if (n == 0)                     return 0.0f;
    if (t <= 0.0f)                  return profile[0];
    float max_t = (float)(n - 1) * step;
    if (t >= max_t)                 return profile[n - 1];
    float f = t / step;
    uint32_t i = (uint32_t)f;
    if (i + 1 >= n) return profile[n - 1];
    float lam = f - (float)i;
    return profile[i] * (1.0f - lam) + profile[i + 1] * lam;
}

/* Rasterise one polyline into ctx->grid. */
static int rasterise_line(planish_ctx *ctx, const osmmesh_planish_line *line)
{
    if (line->n < 2) return 0;

    float *profile = NULL;
    uint32_t n_profile = 0;
    float profile_step = 0.0f;
    float total_len    = 0.0f;
    float *spine_t     = NULL;
    if (build_smoothed_profile(ctx, line, &profile, &n_profile,
                                 &profile_step, &total_len, &spine_t) != 0) {
        return -1;
    }

    float half_w  = 0.5f * line->width_m;
    float falloff = OSMMESH_PLANISH_FALLOFF_FACTOR * half_w;    /* outer radius */
    float fallband = falloff - half_w;                            /* width of blend band */

    /* Per-edge bbox sweep. We iterate edges; a pixel may be visited by
     * multiple edges and each time we keep the closest-segment projection.
     * For correctness with the priority scratch the "smoothed" z for a
     * given pixel is the one from the nearest segment, which is what we
     * compute naturally. */
    for (uint32_t ei = 0; ei + 1 < line->n; ++ei) {
        float x0 = line->xy[ei * 2 + 0];
        float y0 = line->xy[ei * 2 + 1];
        float x1 = line->xy[(ei + 1) * 2 + 0];
        float y1 = line->xy[(ei + 1) * 2 + 1];
        float ex = x1 - x0, ey = y1 - y0;
        float L2 = ex * ex + ey * ey;
        if (L2 < 1e-12f) continue;
        float seg_t0 = spine_t[ei];

        /* Segment bounding box + falloff margin. */
        float xmin = (x0 < x1 ? x0 : x1) - falloff;
        float xmax = (x0 > x1 ? x0 : x1) + falloff;
        float ymin = (y0 < y1 ? y0 : y1) - falloff;
        float ymax = (y0 > y1 ? y0 : y1) + falloff;

        double col_min_d, col_max_d, row_min_d, row_max_d;
        double c_a, r_a, c_b, r_b;
        enu_to_grid(ctx, xmin, ymin, &c_a, &r_a);
        enu_to_grid(ctx, xmax, ymax, &c_b, &r_b);
        /* dy_n is negative, so a larger n gives a SMALLER row. Order both
         * sets so min < max. */
        col_min_d = (c_a < c_b) ? c_a : c_b;
        col_max_d = (c_a > c_b) ? c_a : c_b;
        row_min_d = (r_a < r_b) ? r_a : r_b;
        row_max_d = (r_a > r_b) ? r_a : r_b;

        int32_t c_lo = (int32_t)floor(col_min_d);
        int32_t c_hi = (int32_t)ceil (col_max_d);
        int32_t r_lo = (int32_t)floor(row_min_d);
        int32_t r_hi = (int32_t)ceil (row_max_d);
        if (c_lo < 0)                         c_lo = 0;
        if (c_hi > (int32_t)(ctx->grid->cols - 1)) c_hi = (int32_t)(ctx->grid->cols - 1);
        if (r_lo < 0)                         r_lo = 0;
        if (r_hi > (int32_t)(ctx->grid->rows - 1)) r_hi = (int32_t)(ctx->grid->rows - 1);
        if (c_lo > c_hi || r_lo > r_hi) continue;

        for (int32_t r = r_lo; r <= r_hi; ++r) {
            for (int32_t c = c_lo; c <= c_hi; ++c) {
                float pe, pn;
                grid_to_enu(ctx, (uint32_t)r, (uint32_t)c, &pe, &pn);

                /* Project (pe, pn) onto segment line. */
                float dx = pe - x0, dy = pn - y0;
                float proj = (dx * ex + dy * ey) / L2;
                float tc   = proj;
                if (tc < 0.0f) tc = 0.0f;
                if (tc > 1.0f) tc = 1.0f;
                float cx = x0 + tc * ex;
                float cy = y0 + tc * ey;
                float ddx = pe - cx, ddy = pn - cy;
                float dist = sqrtf(ddx * ddx + ddy * ddy);
                if (dist >= falloff) continue;

                /* Priority gate: drop writes from a lower-priority feature
                 * than what a previous pass already committed to this
                 * pixel. Equal priority permits overwrite (deterministic
                 * within a given pass order; last write wins). */
                size_t pi = (size_t)r * ctx->grid->cols + (size_t)c;
                uint8_t prev = ctx->prio_scratch[pi];
                uint8_t here = (uint8_t)(line->priority & 0xFF);
                if (prev != 0xFF && here < prev) continue;

                /* Smoothed z at this t. Use the perpendicular-foot arc
                 * length (seg_t0 + tc * seg_length). */
                float seg_len = sqrtf(L2);
                float s = seg_t0 + tc * seg_len;
                float sz = sample_profile(profile, n_profile, profile_step, s);

                float new_z;
                if (dist <= half_w) {
                    new_z = sz;
                } else {
                    /* Cosine blend across fallband. */
                    float u = (dist - half_w) / fallband;
                    if (u < 0.0f) u = 0.0f;
                    if (u > 1.0f) u = 1.0f;
                    float w = 0.5f * (1.0f + cosf((float)M_PI * u));
                    /* At u=0 (dist=half_w) w=1 (fully smoothed).
                     * At u=1 (dist=falloff)  w=0 (fully original). */
                    float original_z;
                    /* Priority edge case: when a higher-priority pass
                     * already rewrote this pixel, blending against the
                     * *current* height preserves that; against the RAW
                     * grid would undo it. Use the current height. */
                    original_z = ctx->grid->heights[pi];
                    new_z = w * sz + (1.0f - w) * original_z;
                }

                /* Prefer the closer segment when multiple visit this
                 * pixel within the same polyline — approximate by only
                 * overwriting when the new projection is "strong" (dist
                 * closer to the centreline). Without keeping a distance
                 * buffer we use the rule: always commit; subsequent
                 * same-line segments with a farther projection will see
                 * the pixel already fully smoothed and produce only minor
                 * drift. Acceptable MVP. */
                ctx->grid->heights[pi] = new_z;
                ctx->prio_scratch[pi]  = here;
            }
        }
    }

    free(profile);
    free(spine_t);
    return 0;
}

/* ========================================================================
 *  Driver
 * ====================================================================== */

static int cmp_line_prio(const void *a, const void *b)
{
    const osmmesh_planish_line *la = (const osmmesh_planish_line *)a;
    const osmmesh_planish_line *lb = (const osmmesh_planish_line *)b;
    /* Ascending priority: low class first, so high classes overwrite. */
    if (la->priority < lb->priority) return -1;
    if (la->priority > lb->priority) return  1;
    return 0;
}

/* ========================================================================
 *  Notch: sink heightgrid pixels under linear ribbons by a constant depth
 *  (with cosine falloff beyond the ribbon half-width). Unlike the
 *  smoothed-profile rasteriser this is purely subtractive — the terrain
 *  keeps its natural gradient, it just sits `depth_m` deeper under roads
 *  / waterways. Overlapping features don't stack: max(notch) per pixel.
 *
 *  Cross-tile stable as long as every neighbouring tile sees the same
 *  buffer-zone features in its MVT payload (Planetiler default). Both
 *  tiles write the same delta to the shared edge pixel -> same outcome.
 * ====================================================================== */
int osmmesh_planish_notch_linears(osmmesh_terrain_grid       *grid,
                                   const osmmesh_tile_enu_map *map,
                                   const osmmesh_planish_line_set *lines,
                                   float                       depth_m)
{
    if (!grid || !map || !lines) return -1;
    if (!grid->heights || grid->rows < 2 || grid->cols < 2) return -1;
    if (lines->count == 0 || depth_m <= 0.0f) return 0;

    size_t n_px = (size_t)grid->rows * (size_t)grid->cols;
    float *notch = (float *)calloc(n_px, sizeof(float));   /* max depth per pixel */
    if (!notch) return -1;

    planish_ctx ctx;
    planish_ctx_init(&ctx, grid, map, NULL);

    /* Grid cell diagonal in metres. A ribbon narrower than the cell would
     * be missed by a strict dist<half_width pixel test (centerline passes
     * between two pixel centres, both further than half_width away) and
     * the terrain triangle spanning the band would then pop through it.
     * Clamp the effective falloff to ~1.5 * cell_diagonal so every terrain
     * triangle touched by the ribbon has at least its closest vertex
     * marked. Together with the post-pass dilation below this enforces
     * "every terrain triangle overlapping a linear triangle in 2D gets
     * its full height stack pushed under the ribbon". */
    float cell_diag = sqrtf((float)(ctx.dx_e * ctx.dx_e +
                                     ctx.dy_n * ctx.dy_n));
    float min_falloff = 1.5f * cell_diag;

    for (uint32_t li = 0; li < lines->count; ++li) {
        const osmmesh_planish_line *line = &lines->items[li];
        if (line->n < 2 || line->width_m <= 0.0f) continue;

        float half_w   = 0.5f * line->width_m;
        float falloff  = OSMMESH_PLANISH_FALLOFF_FACTOR * half_w;
        if (falloff < min_falloff) falloff = min_falloff;
        float fallband = falloff - half_w;
        if (fallband < 1e-6f) fallband = 1e-6f;

        for (uint32_t ei = 0; ei + 1 < line->n; ++ei) {
            float x0 = line->xy[ei * 2 + 0];
            float y0 = line->xy[ei * 2 + 1];
            float x1 = line->xy[(ei + 1) * 2 + 0];
            float y1 = line->xy[(ei + 1) * 2 + 1];
            float ex = x1 - x0, ey = y1 - y0;
            float L2 = ex * ex + ey * ey;
            if (L2 < 1e-12f) continue;

            float xmin = (x0 < x1 ? x0 : x1) - falloff;
            float xmax = (x0 > x1 ? x0 : x1) + falloff;
            float ymin = (y0 < y1 ? y0 : y1) - falloff;
            float ymax = (y0 > y1 ? y0 : y1) + falloff;

            double c_a, r_a, c_b, r_b;
            enu_to_grid(&ctx, xmin, ymin, &c_a, &r_a);
            enu_to_grid(&ctx, xmax, ymax, &c_b, &r_b);
            double c_lo_d = (c_a < c_b) ? c_a : c_b;
            double c_hi_d = (c_a > c_b) ? c_a : c_b;
            double r_lo_d = (r_a < r_b) ? r_a : r_b;
            double r_hi_d = (r_a > r_b) ? r_a : r_b;

            int32_t c_lo = (int32_t)floor(c_lo_d);
            int32_t c_hi = (int32_t)ceil (c_hi_d);
            int32_t r_lo = (int32_t)floor(r_lo_d);
            int32_t r_hi = (int32_t)ceil (r_hi_d);
            if (c_lo < 0) c_lo = 0;
            if (c_hi > (int32_t)(grid->cols - 1)) c_hi = (int32_t)(grid->cols - 1);
            if (r_lo < 0) r_lo = 0;
            if (r_hi > (int32_t)(grid->rows - 1)) r_hi = (int32_t)(grid->rows - 1);
            if (c_lo > c_hi || r_lo > r_hi) continue;

            for (int32_t r = r_lo; r <= r_hi; ++r) {
                for (int32_t c = c_lo; c <= c_hi; ++c) {
                    float pe, pn;
                    grid_to_enu(&ctx, (uint32_t)r, (uint32_t)c, &pe, &pn);
                    float dx = pe - x0, dy = pn - y0;
                    float proj = (dx * ex + dy * ey) / L2;
                    if (proj < 0.0f) proj = 0.0f;
                    if (proj > 1.0f) proj = 1.0f;
                    float cx = x0 + proj * ex;
                    float cy = y0 + proj * ey;
                    float ddx = pe - cx, ddy = pn - cy;
                    float dist = sqrtf(ddx * ddx + ddy * ddy);
                    if (dist >= falloff) continue;

                    float amount;
                    if (dist <= half_w) {
                        amount = depth_m;
                    } else {
                        float u = (dist - half_w) / fallband;
                        if (u > 1.0f) u = 1.0f;
                        float w = 0.5f * (1.0f + cosf((float)M_PI * u));
                        amount = depth_m * w;
                    }
                    size_t pi = (size_t)r * grid->cols + (size_t)c;
                    if (amount > notch[pi]) notch[pi] = amount;
                }
            }
        }
    }

    /* Dilation: for every marked pixel propagate its amount to the
     * surrounding NEIGHBOURHOOD via max(). Radius 2 covers the case where
     * a ribbon crosses a slope and the terrain triangle plane between
     * vertices two cells away still overshoots the ribbon plane; cheaper
     * than doing an actual plane-vs-triangle test per cell, and correct
     * as long as the slope is gentle enough that the terrain mesh can't
     * bulge above the ribbon in a 2-cell span by more than the notch
     * depth. */
    {
        const int32_t DIL = 2;
        float *dil = (float *)malloc(n_px * sizeof(float));
        if (dil) {
            memcpy(dil, notch, n_px * sizeof(float));
            uint32_t R = grid->rows, C = grid->cols;
            for (uint32_t r = 0; r < R; ++r) {
                for (uint32_t c = 0; c < C; ++c) {
                    float m = notch[(size_t)r * C + c];
                    if (m <= 0.0f) continue;
                    int32_t r0 = (int32_t)r - DIL, r1 = (int32_t)r + DIL;
                    int32_t c0 = (int32_t)c - DIL, c1 = (int32_t)c + DIL;
                    if (r0 < 0) r0 = 0;
                    if (r1 >= (int32_t)R) r1 = (int32_t)R - 1;
                    if (c0 < 0) c0 = 0;
                    if (c1 >= (int32_t)C) c1 = (int32_t)C - 1;
                    for (int32_t rr = r0; rr <= r1; ++rr) {
                        for (int32_t cc = c0; cc <= c1; ++cc) {
                            size_t pi = (size_t)rr * C + (size_t)cc;
                            if (m > dil[pi]) dil[pi] = m;
                        }
                    }
                }
            }
            free(notch);
            notch = dil;
        }
    }

    for (size_t i = 0; i < n_px; ++i) grid->heights[i] -= notch[i];
    free(notch);
    return 0;
}

/* T14.8: triangle-direct notch using already-built linear meshes.
 *
 * The pipeline now builds linear meshes BEFORE the terrain mesh; their
 * draped z values are the source of truth for the notch. For each linear
 * triangle we sweep its xy bounding box in heightgrid pixels and, for
 * every pixel inside the 2-D triangle, lower the pixel z to at most
 * z_linear(pixel xy) - margin_m. This makes the test_086 invariant
 * (every linear triangle vertex/centroid above terrain mesh) true by
 * construction.
 *
 * Cost is O(sum_triangles * avg_pixels_per_triangle_bbox), comparable
 * to the old polyline-rasterise notch but more accurate (uses the real
 * ribbon geometry including mitres + bridge ramps). */
int osmmesh_terrain_notch_under_linear_meshes(
        osmmesh_terrain_grid       *grid,
        const osmmesh_tile_enu_map *map,
        const osmmesh_mesh         *meshes,
        size_t                      n_meshes,
        float                       margin_m)
{
    if (!grid || !map || !meshes || n_meshes == 0) return 0;
    if (!grid->heights || grid->rows < 2 || grid->cols < 2) return -1;

    planish_ctx ctx;
    planish_ctx_init(&ctx, grid, map, NULL);

    /* Slack for the bary point-in-triangle test. Linear ribbon triangles
     * are typically ~5 m x 0.5 m while heightgrid pixels are ~9 m wide
     * at z=14 -- the pixel CENTRE is almost never strictly inside a
     * narrow ribbon quad, so a 1e-3 slack misses ~all of them and the
     * notch is a no-op. Slack of 1.0 means "any pixel whose centre lies
     * within one full triangle-side of the triangle gets notched with
     * the extrapolated z_linear at that pixel"; combined with the +1
     * BBox padding we cover every pixel a terrain triangle COULD use
     * to span the ribbon. */
    const float SLACK = 1.0f;

    for (size_t mi = 0; mi < n_meshes; ++mi) {
        const osmmesh_mesh *m = &meshes[mi];
        if (!m->positions || !m->indices || m->n_triangles == 0) continue;

        for (uint32_t t = 0; t < m->n_triangles; ++t) {
            uint32_t i0 = m->indices[t*3+0];
            uint32_t i1 = m->indices[t*3+1];
            uint32_t i2 = m->indices[t*3+2];
            float e0 = m->positions[i0*3+0], n0 = m->positions[i0*3+1], z0 = m->positions[i0*3+2];
            float e1 = m->positions[i1*3+0], n1 = m->positions[i1*3+1], z1 = m->positions[i1*3+2];
            float e2 = m->positions[i2*3+0], n2 = m->positions[i2*3+1], z2 = m->positions[i2*3+2];

            float emin = fminf(fminf(e0, e1), e2);
            float emax = fmaxf(fmaxf(e0, e1), e2);
            float nmin = fminf(fminf(n0, n1), n2);
            float nmax = fmaxf(fmaxf(n0, n1), n2);

            double c_a, r_a, c_b, r_b;
            enu_to_grid(&ctx, emin, nmin, &c_a, &r_a);
            enu_to_grid(&ctx, emax, nmax, &c_b, &r_b);
            double c_lo_d = (c_a < c_b) ? c_a : c_b;
            double c_hi_d = (c_a > c_b) ? c_a : c_b;
            double r_lo_d = (r_a < r_b) ? r_a : r_b;
            double r_hi_d = (r_a > r_b) ? r_a : r_b;

            /* Pad 2 cells in every direction so terrain triangles whose
             * far vertices live a cell or two away from the ribbon still
             * have all three corners notched and can't span the band.
             * Combined with SLACK=1.0 every neighbouring pixel inherits
             * the linear z (extrapolated when negative-bary). */
            int32_t c_lo = (int32_t)floor(c_lo_d) - 2;
            int32_t c_hi = (int32_t)ceil (c_hi_d) + 2;
            int32_t r_lo = (int32_t)floor(r_lo_d) - 2;
            int32_t r_hi = (int32_t)ceil (r_hi_d) + 2;
            if (c_lo < 0) c_lo = 0;
            if (c_hi > (int32_t)(grid->cols - 1)) c_hi = (int32_t)(grid->cols - 1);
            if (r_lo < 0) r_lo = 0;
            if (r_hi > (int32_t)(grid->rows - 1)) r_hi = (int32_t)(grid->rows - 1);
            if (c_lo > c_hi || r_lo > r_hi) continue;

            float det = (n1 - n2) * (e0 - e2) + (e2 - e1) * (n0 - n2);
            if (fabsf(det) < 1e-12f) continue;
            float inv_det = 1.0f / det;

            for (int32_t r = r_lo; r <= r_hi; ++r) {
                for (int32_t c = c_lo; c <= c_hi; ++c) {
                    float pe, pn;
                    grid_to_enu(&ctx, (uint32_t)r, (uint32_t)c, &pe, &pn);
                    float w0 = ((n1 - n2) * (pe - e2) + (e2 - e1) * (pn - n2)) * inv_det;
                    float w1 = ((n2 - n0) * (pe - e2) + (e0 - e2) * (pn - n2)) * inv_det;
                    float w2 = 1.0f - w0 - w1;
                    if (w0 < -SLACK || w1 < -SLACK || w2 < -SLACK) continue;
                    float z_at = w0 * z0 + w1 * z1 + w2 * z2;
                    float target = z_at - margin_m;
                    size_t pi = (size_t)r * grid->cols + (size_t)c;
                    if (target < grid->heights[pi]) grid->heights[pi] = target;
                }
            }
        }
    }

    /* Dilation is built into the BBox sweep above (c_lo-1 / c_hi+1 etc.
     * combined with the bary slack) — a separate min-with-neighbours
     * pass would need a notched-mask to avoid propagating natural valley
     * heights into ridges. Skipped for now; if the test still shows
     * "triangle spans ribbon" cases we'll add a tracked-mask dilation. */
    return 0;
}

int osmmesh_planish_rasterise_linears(osmmesh_terrain_grid       *grid,
                                       const osmmesh_tile_enu_map *map,
                                       osmmesh_planish_line_set   *lines)
{
    if (!grid || !map || !lines) return -1;
    if (!grid->heights || grid->rows < 2 || grid->cols < 2) return -1;
    if (lines->count == 0) return 0;

    /* 0xFF sentinel = "nothing has written this pixel yet". We assume
     * priorities fit in [0..254]; osmmesh_linear_class_priority returns
     * values in that range. */
    size_t n_px = (size_t)grid->rows * (size_t)grid->cols;
    uint8_t *scratch = (uint8_t *)malloc(n_px);
    if (!scratch) return -1;
    memset(scratch, 0xFF, n_px);

    qsort(lines->items, lines->count, sizeof(lines->items[0]), cmp_line_prio);

    planish_ctx ctx;
    planish_ctx_init(&ctx, grid, map, scratch);

    int any_fail = 0;
    for (uint32_t i = 0; i < lines->count; ++i) {
        if (rasterise_line(&ctx, &lines->items[i]) != 0) {
            any_fail = 1;
            /* Keep going so we don't leak into a partially-planished state;
             * the caller's flat grid will simply not reflect this feature. */
        }
    }

    free(scratch);
    return any_fail ? -1 : 0;
}
