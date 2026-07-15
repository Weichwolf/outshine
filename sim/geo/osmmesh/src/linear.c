/* libosmmesh/src/linear.c
 *
 * Orchestrator for the T7 linear-feature mesh generator. Owns:
 *   - Classification: Shortbread `kind` tag first, OMT `class` / raw OSM
 *     `highway` fallback.
 *   - Width table: a fixed-size per-class float array.
 *   - Multi-linestring handling: walks each MVT sub-string via ring_offsets
 *     and concatenates ribbons into one mesh.
 *   - Mesh allocation / teardown (shared with T5/T6 via osmmesh_mesh).
 *
 * All heavy geometry lives in linear_ribbon.c.
 */

#include "osmmesh/linear.h"
#include "osmmesh/geo.h"
#include "osmmesh/mesh.h"
#include "osmmesh/mvt.h"
#include "osmmesh/terrain.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linear_internal.h"
#include "terrain_planish.h"

/* ========================================================================
 *  Width table (metres). Indexed by osmmesh_linear_kind.
 *
 *  subway is 0 (underground): callers may elect to skip such features, but
 *  we return a valid kind so they can classify before deciding. For the
 *  `default_width` API we return 0 for UNKNOWN/SUBWAY; the build entry
 *  point treats width < 1e-3 as a zero-width skip.
 * ====================================================================== */

static const float k_width_table[OSMMESH_LINEAR_COUNT] = {
    [OSMMESH_LINEAR_UNKNOWN]     =  0.0f,
    [OSMMESH_LINEAR_MOTORWAY]    = 14.0f,
    [OSMMESH_LINEAR_TRUNK]       = 12.0f,
    [OSMMESH_LINEAR_PRIMARY]     = 10.0f,
    [OSMMESH_LINEAR_SECONDARY]   =  8.0f,
    [OSMMESH_LINEAR_TERTIARY]    =  7.0f,
    [OSMMESH_LINEAR_RESIDENTIAL] =  6.0f,
    [OSMMESH_LINEAR_SERVICE]     =  3.0f,
    [OSMMESH_LINEAR_FOOTWAY]     =  2.0f,
    [OSMMESH_LINEAR_CYCLEWAY]    =  2.0f,
    [OSMMESH_LINEAR_PATH]        =  1.5f,
    [OSMMESH_LINEAR_RAIL]        =  3.0f,
    [OSMMESH_LINEAR_TRAM]        =  2.0f,
    [OSMMESH_LINEAR_SUBWAY]      =  0.0f,
    [OSMMESH_LINEAR_RIVER]       = 25.0f,  /* representative; real rivers are polygons */
    [OSMMESH_LINEAR_CANAL]       =  8.0f,
    [OSMMESH_LINEAR_STREAM]      =  3.0f,
    [OSMMESH_LINEAR_DRAIN]       =  1.5f,
    [OSMMESH_LINEAR_DITCH]       =  1.0f,
};

float osmmesh_linear_default_width(osmmesh_linear_kind kind)
{
    if (kind <= OSMMESH_LINEAR_UNKNOWN || kind >= OSMMESH_LINEAR_COUNT)
        return 0.0f;
    return k_width_table[kind];
}

/* Max allowed terrain-following gradient per class (rise over run).
 * Above this the segment is treated as a bridge or tunnel: ribbon stays
 * on the linear interpolation between the original polyline endpoints
 * instead of draping. Rail is very low on purpose (adhesion traction
 * tops out around 2-3 %); waterways effectively flat because water
 * doesn't flow uphill. Numbers intentionally slightly above typical
 * max so realistic mapped routes still drape. */
static float class_max_gradient(osmmesh_linear_kind kind)
{
    switch (kind) {
    case OSMMESH_LINEAR_MOTORWAY:    return 0.06f;
    case OSMMESH_LINEAR_TRUNK:       return 0.06f;
    case OSMMESH_LINEAR_PRIMARY:     return 0.08f;
    case OSMMESH_LINEAR_SECONDARY:   return 0.08f;
    case OSMMESH_LINEAR_TERTIARY:    return 0.10f;
    case OSMMESH_LINEAR_RESIDENTIAL: return 0.12f;
    case OSMMESH_LINEAR_SERVICE:     return 0.15f;
    case OSMMESH_LINEAR_FOOTWAY:     return 0.30f;
    case OSMMESH_LINEAR_CYCLEWAY:    return 0.06f;
    case OSMMESH_LINEAR_PATH:        return 0.30f;
    case OSMMESH_LINEAR_RAIL:        return 0.025f;
    case OSMMESH_LINEAR_TRAM:        return 0.07f;
    case OSMMESH_LINEAR_SUBWAY:      return 0.03f;
    case OSMMESH_LINEAR_RIVER:       return 0.01f;
    case OSMMESH_LINEAR_CANAL:       return 0.005f;
    case OSMMESH_LINEAR_STREAM:      return 0.02f;
    case OSMMESH_LINEAR_DRAIN:       return 0.03f;
    case OSMMESH_LINEAR_DITCH:       return 0.03f;
    default:                          return 0.20f;
    }
}

/* Class priority for terrain-planishing arbitration. Higher wins on overlap.
 * Values are picked from the T12 spec:
 *   motorway=10, trunk=9, primary=8, secondary=7, tertiary=6,
 *   residential=5, service=4, rail=4, tram=3, cycleway=2, footway=1,
 *   path=1, subway=-1 (underground; caller should skip).
 *
 * UNKNOWN returns -1 so callers don't planish unclassified features.
 *
 * The magnitude is opaque — do not persist it across binary revisions. */
int osmmesh_linear_class_priority(osmmesh_linear_kind kind)
{
    switch (kind) {
    case OSMMESH_LINEAR_MOTORWAY:    return 10;
    case OSMMESH_LINEAR_TRUNK:       return  9;
    case OSMMESH_LINEAR_PRIMARY:     return  8;
    case OSMMESH_LINEAR_SECONDARY:   return  7;
    case OSMMESH_LINEAR_TERTIARY:    return  6;
    case OSMMESH_LINEAR_RESIDENTIAL: return  5;
    case OSMMESH_LINEAR_SERVICE:     return  4;
    case OSMMESH_LINEAR_RAIL:        return  4;
    case OSMMESH_LINEAR_TRAM:        return  3;
    case OSMMESH_LINEAR_CYCLEWAY:    return  2;
    case OSMMESH_LINEAR_FOOTWAY:     return  1;
    case OSMMESH_LINEAR_PATH:        return  1;
    case OSMMESH_LINEAR_SUBWAY:      return -1;  /* underground: skip */
    /* Waterways: priority 0 — they don't planish over roads (would be
     * absurd: rivers don't grade hills). Will matter once T12 planishing
     * comes back. */
    case OSMMESH_LINEAR_RIVER:
    case OSMMESH_LINEAR_CANAL:
    case OSMMESH_LINEAR_STREAM:
    case OSMMESH_LINEAR_DRAIN:
    case OSMMESH_LINEAR_DITCH:       return  0;
    case OSMMESH_LINEAR_UNKNOWN:
    case OSMMESH_LINEAR_COUNT:
    default:                         return -1;
    }
}

/* ========================================================================
 *  Classification
 *
 *  Tag lookup probes, in order:
 *    1. `kind`      — Shortbread 1.0 schema's canonical class tag.
 *    2. `class`     — OpenMapTiles / planetiler-openmaptiles convention.
 *    3. `highway`   — raw OSM tag (carried through by some bespoke
 *                     profiles). Same spelling as the OSM key.
 *    4. `railway`   — raw OSM key for rail/tram/subway when the producer
 *                     didn't normalise them into `kind` / `class`.
 *
 *  String compare is case-sensitive (Shortbread / OMT / OSM all emit
 *  lowercase). Unknown values collapse to UNKNOWN.
 * ====================================================================== */

static const struct { const char *key; osmmesh_linear_kind kind; } k_name_to_kind[] = {
    { "motorway",    OSMMESH_LINEAR_MOTORWAY    },
    { "trunk",       OSMMESH_LINEAR_TRUNK       },
    { "primary",     OSMMESH_LINEAR_PRIMARY     },
    { "secondary",   OSMMESH_LINEAR_SECONDARY   },
    { "tertiary",    OSMMESH_LINEAR_TERTIARY    },
    { "residential", OSMMESH_LINEAR_RESIDENTIAL },
    { "living_street", OSMMESH_LINEAR_RESIDENTIAL }, /* quasi-residential */
    { "unclassified",  OSMMESH_LINEAR_RESIDENTIAL }, /* Shortbread bucket */
    { "service",     OSMMESH_LINEAR_SERVICE     },
    { "footway",     OSMMESH_LINEAR_FOOTWAY     },
    { "pedestrian",  OSMMESH_LINEAR_FOOTWAY     },
    { "steps",       OSMMESH_LINEAR_FOOTWAY     },
    { "cycleway",    OSMMESH_LINEAR_CYCLEWAY    },
    { "path",        OSMMESH_LINEAR_PATH        },
    { "track",       OSMMESH_LINEAR_PATH        },
    { "rail",        OSMMESH_LINEAR_RAIL        },
    { "tram",        OSMMESH_LINEAR_TRAM        },
    { "light_rail",  OSMMESH_LINEAR_TRAM        },
    { "subway",      OSMMESH_LINEAR_SUBWAY      },
    /* Shortbread water_lines + raw OSM waterway= values. */
    { "river",       OSMMESH_LINEAR_RIVER       },
    { "canal",       OSMMESH_LINEAR_CANAL       },
    { "stream",      OSMMESH_LINEAR_STREAM      },
    { "drain",       OSMMESH_LINEAR_DRAIN       },
    { "ditch",       OSMMESH_LINEAR_DITCH       },
};

static osmmesh_linear_kind kind_from_name(const char *s)
{
    if (!s) return OSMMESH_LINEAR_UNKNOWN;
    size_t n = sizeof(k_name_to_kind) / sizeof(k_name_to_kind[0]);
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(s, k_name_to_kind[i].key) == 0)
            return k_name_to_kind[i].kind;
    }
    return OSMMESH_LINEAR_UNKNOWN;
}

static int get_string_tag(const osmmesh_mvt_layer *L,
                           const osmmesh_mvt_feature *f,
                           const char *key, const char **out)
{
    osmmesh_mvt_value v;
    if (osmmesh_mvt_feature_get_tag(L, f, key, &v) != 0) return 0;
    if (v.type != OSMMESH_MVT_VAL_STRING || !v.v.s) return 0;
    *out = v.v.s;
    return 1;
}

osmmesh_linear_kind osmmesh_linear_classify(const osmmesh_mvt_layer *layer,
                                              const osmmesh_mvt_feature *feature)
{
    if (!layer || !feature) return OSMMESH_LINEAR_UNKNOWN;
    const char *probes[] = { "kind", "class", "highway", "railway", "waterway" };
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
        const char *s = NULL;
        if (get_string_tag(layer, feature, probes[i], &s)) {
            osmmesh_linear_kind k = kind_from_name(s);
            if (k != OSMMESH_LINEAR_UNKNOWN) return k;
        }
    }
    return OSMMESH_LINEAR_UNKNOWN;
}

/* ========================================================================
 *  Build driver
 * ====================================================================== */

int osmmesh_linear_build(const osmmesh_mvt_layer *layer,
                          const osmmesh_mvt_feature *feature,
                          const osmmesh_tile_enu_map *map,
                          const osmmesh_linear_opts *opts,
                          osmmesh_mesh *out_mesh,
                          osmmesh_linear_info *out_info)
{
    /* Public entry: no terrain. Legacy flat-on-Z=0 ribbon. */
    return osmmesh_linear_build_with_terrain(layer, feature, map, opts,
                                               NULL, 0.0f,
                                               out_mesh, out_info);
}

int osmmesh_linear_build_with_terrain(
    const osmmesh_mvt_layer     *layer,
    const osmmesh_mvt_feature   *feature,
    const osmmesh_tile_enu_map  *map,
    const osmmesh_linear_opts   *opts,
    const osmmesh_terrain_grid  *terrain_grid,
    float                        clearance_m,
    osmmesh_mesh                *out_mesh,
    osmmesh_linear_info         *out_info)
{
    if (!layer || !feature || !map || !out_mesh) return OSMMESH_LINEAR_ERR_ARG;

    memset(out_mesh, 0, sizeof *out_mesh);
    if (out_info) memset(out_info, 0, sizeof *out_info);

    if (feature->geom_type != OSMMESH_MVT_GEOM_LINESTRING)
        return OSMMESH_LINEAR_ERR_SKIP;
    if (feature->n_coords < 2) return OSMMESH_LINEAR_ERR_SKIP;

    /* Resolve classification + width. */
    osmmesh_linear_kind kind = osmmesh_linear_classify(layer, feature);

    float width = 0.0f;
    if (opts && opts->width_override_m > 0.0f) {
        width = opts->width_override_m;
    } else {
        if (kind == OSMMESH_LINEAR_UNKNOWN) return OSMMESH_LINEAR_ERR_SKIP;
        width = osmmesh_linear_default_width(kind);
    }
    if (width < 1e-3f) return OSMMESH_LINEAR_ERR_SKIP;

    float mitre_limit = (opts && opts->mitre_limit > 0.0f) ? opts->mitre_limit : 4.0f;
    /* UVs default ON. The opts struct field is tri-state: undefined/0 = on
     * (default), explicit -1 = off. Kept as plain int to avoid a public-API
     * break; 0-init of osmmesh_linear_opts now means "give me UVs" which
     * matches the new lib-wide contract. */
    int   emit_uvs    = (opts && opts->emit_uvs < 0) ? 0 : 1;
    float min_length  = (opts && opts->min_length_m > 0.0f) ? opts->min_length_m : 0.0f;

    /* Enumerate sub-strings. MVT LINESTRING uses ring_offsets where each
     * entry is the start coord index of a sub-string; for features with
     * a single MoveTo, n_rings == 1 and ring_offsets = [0, n_coords]. */
    uint32_t n_sub = (uint32_t)(feature->n_rings ? feature->n_rings : 1);
    const uint32_t *ring_offsets = feature->ring_offsets;
    uint32_t fallback_ranges[2] = { 0, (uint32_t)feature->n_coords };
    if (!ring_offsets) ring_offsets = fallback_ranges;

    /* Project all coords once into a local ENU buffer. Keep sub-string
     * ranges in-place; we'll slice when emitting. */
    float *xy = (float *)malloc((size_t)feature->n_coords * 2 * sizeof(float));
    if (!xy) return OSMMESH_LINEAR_ERR_OOM;

    for (size_t i = 0; i < feature->n_coords; ++i) {
        osmmesh_enu p = osmmesh_tile_enu_map_apply(map,
                                                     feature->coords[i].x,
                                                     feature->coords[i].y);
        xy[i * 2 + 0] = (float)p.e;
        xy[i * 2 + 1] = (float)p.n;
    }

    /* Densify per-substring spine so ribbon sampling is at least as fine
     * as the heightgrid. Without this, the ribbon interpolates linearly
     * between far-apart MVT vertices while the terrain mesh follows each
     * heightgrid cell — a hill between two polyline vertices pokes
     * through the ribbon. Sample step = 0.8 * cell_size so we can't skip
     * any cell. No terrain grid -> no densification (keeps classic
     * behaviour for callers without height data). */
    float sample_step = 0.0f;
    if (terrain_grid) {
        float dx_m = 0.0f, dy_m = 0.0f;
        osmmesh_planish_grid_cell_size(terrain_grid, map, &dx_m, &dy_m);
        float cell = (dx_m > dy_m) ? dx_m : dy_m;
        if (cell > 0.0f) sample_step = 0.8f * cell;
    }

    /* Per-substring dense arrays. Each substring keeps a contiguous xy
     * buffer + an owner-edge index per dense vertex so the gradient
     * limiter knows which original MVT edge produced each intermediate. */
    typedef struct {
        float    *xy;           /* dense_n * 2 floats */
        uint32_t *owner_edge;   /* dense_n uint32: which MVT edge (0..n_mvt-2) */
        uint32_t  dense_n;
        uint32_t  n_edges;      /* = b - a - 1, same as original MVT edge count */
        float    *edge_len;     /* n_edges, metres */
        uint32_t *edge_end;     /* n_edges: dense index of edge's end vertex */
    } sub_dense;
    sub_dense *subs = (sub_dense *)calloc((size_t)n_sub, sizeof *subs);
    if (!subs) { free(xy); return OSMMESH_LINEAR_ERR_OOM; }

    for (uint32_t s = 0; s < n_sub; ++s) {
        uint32_t a = ring_offsets[s];
        uint32_t b = ring_offsets[s + 1];
        if (b <= a || (b - a) < 2) continue;

        uint32_t n_mvt  = b - a;
        uint32_t n_edges = n_mvt - 1;

        /* First pass: count dense vertices we'll emit. */
        uint32_t dense_n = 1; /* first vertex */
        for (uint32_t k = 0; k < n_edges; ++k) {
            const float *v0 = &xy[(a + k    ) * 2];
            const float *v1 = &xy[(a + k + 1) * 2];
            float dx = v1[0] - v0[0], dy = v1[1] - v0[1];
            float L = sqrtf(dx * dx + dy * dy);
            uint32_t steps = 1;
            if (sample_step > 0.0f && L > sample_step) {
                steps = (uint32_t)ceilf(L / sample_step);
            }
            dense_n += steps;
        }

        subs[s].xy         = (float *)   malloc((size_t)dense_n * 2 * sizeof(float));
        subs[s].owner_edge = (uint32_t *)malloc((size_t)dense_n     * sizeof(uint32_t));
        subs[s].edge_len   = (float *)   malloc((size_t)n_edges     * sizeof(float));
        subs[s].edge_end   = (uint32_t *)malloc((size_t)n_edges     * sizeof(uint32_t));
        if (!subs[s].xy || !subs[s].owner_edge ||
            !subs[s].edge_len || !subs[s].edge_end) {
            for (uint32_t t = 0; t <= s; ++t) {
                free(subs[t].xy); free(subs[t].owner_edge);
                free(subs[t].edge_len); free(subs[t].edge_end);
            }
            free(subs); free(xy);
            return OSMMESH_LINEAR_ERR_OOM;
        }
        subs[s].dense_n = dense_n;
        subs[s].n_edges = n_edges;

        uint32_t di = 0;
        subs[s].xy[0] = xy[a * 2 + 0];
        subs[s].xy[1] = xy[a * 2 + 1];
        subs[s].owner_edge[di] = 0;
        ++di;
        for (uint32_t k = 0; k < n_edges; ++k) {
            const float *v0 = &xy[(a + k    ) * 2];
            const float *v1 = &xy[(a + k + 1) * 2];
            float dx = v1[0] - v0[0], dy = v1[1] - v0[1];
            float L = sqrtf(dx * dx + dy * dy);
            subs[s].edge_len[k] = L;
            uint32_t steps = 1;
            if (sample_step > 0.0f && L > sample_step) {
                steps = (uint32_t)ceilf(L / sample_step);
            }
            for (uint32_t st = 1; st <= steps; ++st) {
                float t = (float)st / (float)steps;
                subs[s].xy[di * 2 + 0] = v0[0] + t * dx;
                subs[s].xy[di * 2 + 1] = v0[1] + t * dy;
                subs[s].owner_edge[di] = k;
                ++di;
            }
            subs[s].edge_end[k] = di - 1;
        }
    }

    /* Sum over sub-strings using DENSE spine counts. */
    uint32_t v_cap = 0, t_cap = 0;
    uint32_t usable_sub = 0;
    for (uint32_t s = 0; s < n_sub; ++s) {
        if (subs[s].dense_n < 2) continue;
        v_cap += osmmesh_linear_ribbon_vcap(subs[s].dense_n);
        t_cap += osmmesh_linear_ribbon_tcap(subs[s].dense_n);
        ++usable_sub;
    }

#define FREE_SUBS_AND_XY() do { \
        for (uint32_t _i = 0; _i < n_sub; ++_i) { \
            free(subs[_i].xy); free(subs[_i].owner_edge); \
            free(subs[_i].edge_len); free(subs[_i].edge_end); \
        } \
        free(subs); free(xy); \
    } while (0)

    if (usable_sub == 0 || v_cap == 0 || t_cap == 0) {
        FREE_SUBS_AND_XY();
        return OSMMESH_LINEAR_ERR_SKIP;
    }

    float    *pos = (float *)   malloc((size_t)v_cap * 3 * sizeof(float));
    float    *nrm = (float *)   malloc((size_t)v_cap * 3 * sizeof(float));
    float    *uvs = emit_uvs ? (float *)malloc((size_t)v_cap * 2 * sizeof(float)) : NULL;
    uint32_t *idx = (uint32_t *)malloc((size_t)t_cap * 3 * sizeof(uint32_t));
    if (!pos || !nrm || !idx || (emit_uvs && !uvs)) {
        free(pos); free(nrm); free(uvs); free(idx);
        FREE_SUBS_AND_XY();
        return OSMMESH_LINEAR_ERR_OOM;
    }

    osmmesh_linear_emit e = {
        .pos = pos, .nrm = nrm, .uvs = uvs, .idx = idx,
        .v_count = 0, .v_cap = v_cap,
        .t_count = 0, .t_cap = t_cap,
    };

    float total_len = 0.0f;
    uint32_t emitted_sub = 0;
    uint32_t total_seg = 0;
    uint32_t *sub_v_start = (uint32_t *)calloc(n_sub, sizeof(uint32_t));
    uint32_t *sub_v_end   = (uint32_t *)calloc(n_sub, sizeof(uint32_t));

    for (uint32_t s = 0; s < n_sub; ++s) {
        if (subs[s].dense_n < 2) continue;
        sub_v_start[s] = e.v_count;
        float len = osmmesh_linear_ribbon_emit(&e, subs[s].xy, subs[s].dense_n,
                                                width, mitre_limit, emit_uvs);
        sub_v_end[s] = e.v_count;
        if (len <= 0.0f) continue;

        total_len += len;
        total_seg += (subs[s].n_edges);
        ++emitted_sub;
    }

    if (emitted_sub == 0) {
        free(pos); free(nrm); free(uvs); free(idx);
        free(sub_v_start); free(sub_v_end);
        FREE_SUBS_AND_XY();
        return OSMMESH_LINEAR_ERR_SKIP;
    }
    if (min_length > 0.0f && total_len < min_length) {
        free(pos); free(nrm); free(uvs); free(idx);
        free(sub_v_start); free(sub_v_end);
        FREE_SUBS_AND_XY();
        return OSMMESH_LINEAR_ERR_SKIP;
    }

    /* T12 vertex drape with class-aware bridge / tunnel limit.
     *
     * Per ribbon vertex: nearest-spine projection gives us the substring
     * + its owning original MVT edge. We compute the natural drape z =
     * raw_terrain(xy) + clearance. If the original-edge endpoint-to-
     * endpoint gradient (in drape z over edge length) exceeds
     * class_max_gradient(kind), we treat the segment as a bridge or
     * tunnel: every dense vertex inside it gets z replaced by linear
     * interpolation between its two endpoint drape-z values along the
     * spine arc. Realistic mapped routes still drape; over-steep routes
     * stay on the straight ramp the polyline implies. */
    if (terrain_grid) {
        float max_grad = class_max_gradient(kind);

        /* Pre-compute per-substring per-dense-vertex drape z + arc-length
         * from substring start, for quick lookup in the ribbon-vertex
         * post-pass. */
        for (uint32_t s = 0; s < n_sub; ++s) {
            if (subs[s].dense_n < 2) continue;
            uint32_t dn = subs[s].dense_n;
            float *spine_z = (float *)malloc((size_t)dn * sizeof(float));
            float *spine_t = (float *)malloc((size_t)dn * sizeof(float));
            if (!spine_z || !spine_t) { free(spine_z); free(spine_t); continue; }

            spine_t[0] = 0.0f;
            for (uint32_t i = 0; i < dn; ++i) {
                float px = subs[s].xy[i * 2 + 0];
                float py = subs[s].xy[i * 2 + 1];
                spine_z[i] = osmmesh_planish_sample_bilinear(
                                  terrain_grid, map, px, py) + clearance_m;
                if (i > 0) {
                    float dx = subs[s].xy[i*2+0] - subs[s].xy[(i-1)*2+0];
                    float dy = subs[s].xy[i*2+1] - subs[s].xy[(i-1)*2+1];
                    spine_t[i] = spine_t[i-1] + sqrtf(dx*dx + dy*dy);
                }
            }

            /* Per original MVT edge: bridge / tunnel test. Bridge =
             * linear ramp between the endpoint drape z's. The ramp is a
             * MINIMUM, never a replacement: if the natural drape rises
             * above the ramp inside the edge (terrain hill between the
             * polyline endpoints) we keep the drape, otherwise we lift
             * to the ramp. That keeps `every linear vertex >= terrain`
             * while still smoothing over-steep gradients to match what
             * a real bridge / cutting would do.
             *
             * (T14.7 caught this: the previous version REPLACED drape
             * with the ramp, which sank rivers and steep roads into
             * hills whenever the linear interp ran below terrain.) */
            uint32_t prev_end = 0;
            for (uint32_t k = 0; k < subs[s].n_edges; ++k) {
                uint32_t e_end = subs[s].edge_end[k];
                float L = subs[s].edge_len[k];
                if (L <= 1e-3f) { prev_end = e_end; continue; }
                float zA = spine_z[prev_end];
                float zB = spine_z[e_end];
                float grad = fabsf(zB - zA) / L;
                if (grad > max_grad) {
                    float tA = spine_t[prev_end];
                    for (uint32_t i = prev_end + 1; i < e_end; ++i) {
                        float u = (spine_t[i] - tA) / L;
                        if (u < 0.0f) u = 0.0f;
                        if (u > 1.0f) u = 1.0f;
                        float ramp_z = zA + u * (zB - zA);
                        if (ramp_z > spine_z[i]) spine_z[i] = ramp_z;
                    }
                }
                prev_end = e_end;
            }

            /* Apply z to the ribbon vertices we emitted for this
             * substring. Each ribbon vertex sits at one of the dense
             * spine xy positions (left / right offset) — find the closest
             * spine vertex by xy and copy its z. With dense_n >= 2 and
             * the small xy offset width/2 (perpendicular to the spine)
             * this is a unique nearest-spine match per ribbon vertex. */
            uint32_t v0 = sub_v_start[s];
            uint32_t v1 = sub_v_end[s];
            for (uint32_t v = v0; v < v1; ++v) {
                float px = pos[v * 3 + 0];
                float py = pos[v * 3 + 1];
                /* Linear scan; dense_n is small per substring (at z=14
                 * ~10-50). Could bsearch on spine_t once we sort, but
                 * not worth it for MVP. */
                float best_d2 = FLT_MAX;
                uint32_t best_i = 0;
                for (uint32_t i = 0; i < dn; ++i) {
                    float dx = subs[s].xy[i*2+0] - px;
                    float dy = subs[s].xy[i*2+1] - py;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < best_d2) { best_d2 = d2; best_i = i; }
                }
                pos[v * 3 + 2] = spine_z[best_i];
            }
            free(spine_z); free(spine_t);
        }
    }

    free(sub_v_start); free(sub_v_end);
    FREE_SUBS_AND_XY();
#undef FREE_SUBS_AND_XY

    out_mesh->positions   = pos;
    out_mesh->normals     = nrm;
    out_mesh->uvs         = uvs;
    out_mesh->indices     = idx;
    out_mesh->n_vertices  = e.v_count;
    out_mesh->n_triangles = e.t_count;

    if (out_info) {
        out_info->kind         = kind;
        out_info->width_m      = width;
        out_info->length_m     = total_len;
        out_info->n_substrings = emitted_sub;
        out_info->n_segments   = total_seg;
    }
    return OSMMESH_LINEAR_OK;
}
