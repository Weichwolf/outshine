/* libosmmesh/src/building.c
 *
 * Orchestrator for the building-mesh generator. Owns:
 *   - OSM-tag parsing (height, building:levels, roof:shape, roof:height)
 *   - Heuristic dispatch (rectangularity + RNG -> gabled/hipped/flat)
 *   - Terrain drape sampling (T12)
 *   - Seeded RNG (osm_id or coord-stream hash)
 *   - UV post-pass from (position, normal) — T14
 *
 * The actual wall+roof mesh emission lives in building_roof.c (T15.2
 * rewrite). That module uses building_emit.c helpers — the key topology
 * invariant is that walls and the neighbouring roof face SHARE their
 * eave-edge vertex coordinates by construction, so test_087's closed-
 * mesh assertion holds without a post-hoc stitching pass.
 *
 * See building.h for the CW/CCW winding derivation; T15.2 still respects
 * the same conventions but now the responsibility for picking the correct
 * winding per emitted triangle is delegated to building_emit.c's
 * face-up / outward helpers.
 */

#include "osmmesh/building.h"
#include "osmmesh/mvt.h"
#include "osmmesh/mesh.h"
#include "osmmesh/terrain.h"

#include "building_internal.h"
#include "building_emit.h"
#include "terrain_planish.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declared in building_roof.c. */
int osmmesh_building_roof_build_dispatch(
    osmmesh_emit_buf *b,
    const osmmesh_building_footprint *fp,
    osmmesh_roof_shape shape,
    float base_z,
    float wall_h,
    float roof_h,
    const float *vertex_bot_z);

/* ========================================================================
 *  Tag helpers
 * ====================================================================== */

static int tag_str_equals(const osmmesh_mvt_value *v, const char *s)
{
    return v->type == OSMMESH_MVT_VAL_STRING && v->v.s && strcmp(v->v.s, s) == 0;
}

static int tag_get_float(const osmmesh_mvt_layer *L,
                           const osmmesh_mvt_feature *f,
                           const char *key, float *out)
{
    osmmesh_mvt_value v;
    if (osmmesh_mvt_feature_get_tag(L, f, key, &v) != 0) return 0;
    switch (v.type) {
    case OSMMESH_MVT_VAL_FLOAT:  *out = v.v.f; return 1;
    case OSMMESH_MVT_VAL_DOUBLE: *out = (float)v.v.d; return 1;
    case OSMMESH_MVT_VAL_INT:    *out = (float)v.v.i; return 1;
    case OSMMESH_MVT_VAL_UINT:   *out = (float)v.v.u; return 1;
    case OSMMESH_MVT_VAL_SINT:   *out = (float)v.v.z; return 1;
    case OSMMESH_MVT_VAL_STRING:
        if (!v.v.s) return 0;
        /* atof tolerates "12.5", "12", "12.5m" (stops at non-numeric). */
        *out = (float)atof(v.v.s);
        return (*out > 0.0f) ? 1 : 0;
    default: return 0;
    }
}

static int tag_get_roof_shape(const osmmesh_mvt_layer *L,
                                const osmmesh_mvt_feature *f,
                                osmmesh_roof_shape *out)
{
    osmmesh_mvt_value v;
    if (osmmesh_mvt_feature_get_tag(L, f, "roof:shape", &v) != 0) return 0;
    if (tag_str_equals(&v, "flat"))   { *out = OSMMESH_ROOF_FLAT;   return 1; }
    if (tag_str_equals(&v, "gabled")) { *out = OSMMESH_ROOF_GABLED; return 1; }
    if (tag_str_equals(&v, "hipped")) { *out = OSMMESH_ROOF_HIPPED; return 1; }
    /* Unknown shape: caller falls back by heuristic. */
    return 0;
}

/* ========================================================================
 *  Heuristic: pick roof shape + heights
 * ====================================================================== */

typedef struct {
    osmmesh_roof_shape shape;
    float wall_h;
    float roof_h;
} building_decision;

static float default_roof_height(osmmesh_roof_shape s, float obb_short)
{
    switch (s) {
    case OSMMESH_ROOF_FLAT:   return 0.0f;
    case OSMMESH_ROOF_GABLED: return obb_short * 0.25f;
    case OSMMESH_ROOF_HIPPED: return obb_short * 0.30f;
    default: return 0.0f;
    }
}

static void decide(const osmmesh_mvt_layer *L,
                    const osmmesh_mvt_feature *f,
                    const osmmesh_building_footprint *fp,
                    const osmmesh_building_opts *opts,
                    uint64_t seed_fallback,
                    building_decision *out)
{
    osmmesh_building_rng rng;
    uint64_t seed = f->id ? f->id : seed_fallback;
    osmmesh_building_rng_seed(&rng, seed);

    /* ---------- Roof shape ---------- */
    osmmesh_roof_shape shape = OSMMESH_ROOF_FLAT;

    if (opts && opts->roof_shape_override_set) {
        shape = opts->roof_shape_override;
        /* Downgrade to FLAT for non-rectangular shapes that need the OBB
         * pipeline — we promised MVP-strict. */
        if ((shape == OSMMESH_ROOF_GABLED || shape == OSMMESH_ROOF_HIPPED)
            && !fp->is_rectangular) {
            shape = OSMMESH_ROOF_FLAT;
        }
    } else {
        osmmesh_roof_shape tagged;
        if (tag_get_roof_shape(L, f, &tagged)) {
            /* Tag wins, but still gated on rectangularity for pitched. */
            if ((tagged == OSMMESH_ROOF_GABLED || tagged == OSMMESH_ROOF_HIPPED)
                && !fp->is_rectangular) {
                shape = OSMMESH_ROOF_FLAT;
            } else {
                shape = tagged;
            }
        } else if (fp->is_rectangular) {
            float ar = fp->obb_long / (fp->obb_short > 0 ? fp->obb_short : 1.0f);
            float gabled_p = (ar < 1.5f) ? 0.60f : 0.70f;
            float r = osmmesh_building_rng_f01(&rng);
            shape = (r < gabled_p) ? OSMMESH_ROOF_GABLED : OSMMESH_ROOF_HIPPED;
        } else {
            shape = OSMMESH_ROOF_FLAT;   /* regular-but-not-rect and irregular */
        }
    }

    /* ---------- Heights ---------- */
    float total_h = 0.0f;           /* tagged `height` (eaves+ridge) if any */
    int   have_total = tag_get_float(L, f, "height", &total_h);

    float tagged_roof_h = 0.0f;
    int   have_tagged_roof = tag_get_float(L, f, "roof:height", &tagged_roof_h);

    float wall_h = 0.0f;
    float roof_h = 0.0f;

    if (opts && opts->roof_height_override_m > 0.0f) {
        roof_h = opts->roof_height_override_m;
    } else if (have_tagged_roof) {
        roof_h = tagged_roof_h;
    } else {
        roof_h = default_roof_height(shape, fp->obb_short);
    }
    if (shape == OSMMESH_ROOF_FLAT) roof_h = 0.0f;

    if (opts && opts->wall_height_override_m > 0.0f) {
        wall_h = opts->wall_height_override_m;
    } else if (have_total) {
        wall_h = total_h - roof_h;
        if (wall_h < 3.0f) wall_h = 3.0f;
    } else {
        float levels = 0.0f;
        float floor_h = (opts && opts->floor_height_m > 0.0f)
                        ? opts->floor_height_m : 3.0f;
        if (tag_get_float(L, f, "building:levels", &levels) && levels > 0.0f) {
            wall_h = levels * floor_h;
        } else {
            /* Seeded storeys 1..4. */
            uint32_t n_lvls = 1 + osmmesh_building_rng_u32(&rng, 4);
            wall_h = (float)n_lvls * floor_h;
        }
    }
    if (wall_h < 1.0f) wall_h = 1.0f;

    out->shape  = shape;
    out->wall_h = wall_h;
    out->roof_h = roof_h;
}

/* ========================================================================
 *  Public entry
 * ====================================================================== */

int osmmesh_building_build(const osmmesh_mvt_layer    *layer,
                             const osmmesh_mvt_feature *feature,
                             const osmmesh_tile_enu_map *map,
                             const osmmesh_building_opts *opts,
                             osmmesh_mesh              *out_mesh,
                             osmmesh_building_info     *out_info)
{
    /* Public entry: no terrain, legacy flat-on-Z=0 behaviour. */
    return osmmesh_building_build_with_terrain(layer, feature, map, opts,
                                                 NULL, out_mesh, out_info);
}

int osmmesh_building_build_with_terrain(
    const osmmesh_mvt_layer     *layer,
    const osmmesh_mvt_feature   *feature,
    const osmmesh_tile_enu_map  *map,
    const osmmesh_building_opts *opts,
    const osmmesh_terrain_grid  *terrain_grid,
    osmmesh_mesh                *out_mesh,
    osmmesh_building_info       *out_info)
{
    if (!layer || !feature || !map || !out_mesh) return OSMMESH_BUILDING_ERR_ARG;

    memset(out_mesh, 0, sizeof *out_mesh);
    if (out_info) memset(out_info, 0, sizeof *out_info);

    osmmesh_building_footprint fp;
    int rc = osmmesh_building_footprint_build(feature, map, &fp);
    if (rc != OSMMESH_BUILDING_OK) return rc;

    /* Seed fallback hash: over the raw MVT coords (int32 stream). */
    uint64_t seed_fallback = osmmesh_building_rng_hash_u32(
        (const uint32_t *)feature->coords,
        feature->n_coords * 2);


    building_decision dec;
    decide(layer, feature, &fp, opts, seed_fallback, &dec);

    /* T12 terrain anchor. With no grid, everything stays at z=0 (legacy).
     * With a grid: base_z = max(terrain_at(xy_i)), wall bottom per-vertex
     * follows the terrain. */
    float  base_z = 0.0f;
    float *vertex_bot_z = NULL;
    if (terrain_grid) {
        vertex_bot_z = (float *)malloc((size_t)fp.n * sizeof(float));
        if (!vertex_bot_z) return OSMMESH_BUILDING_ERR_OOM;
        float max_z = -FLT_MAX;
        for (uint32_t i = 0; i < fp.n; ++i) {
            float z = osmmesh_planish_sample_bilinear(terrain_grid, map,
                                                       fp.xy[i * 2 + 0],
                                                       fp.xy[i * 2 + 1]);
            vertex_bot_z[i] = z;
            if (z > max_z) max_z = z;
        }
        base_z = max_z;
    }

    osmmesh_emit_buf eb;
    osmmesh_emit_buf_init(&eb);

    int rb = osmmesh_building_roof_build_dispatch(&eb, &fp, dec.shape,
                                                    base_z, dec.wall_h, dec.roof_h,
                                                    vertex_bot_z);
    free(vertex_bot_z);

    if (rb != 0 || eb.nv == 0 || eb.nt == 0) {
        osmmesh_emit_buf_free(&eb);
        return (rb == 0) ? OSMMESH_BUILDING_ERR_SKIP : OSMMESH_BUILDING_ERR_OOM;
    }

    /* Derive UVs from (position, normal) in a post-pass so the per-module
     * emit signatures stay untouched. Convention:
     *   - Roof-ish vertex (|n.z| > 0.5):   u=x,  v=y  (world-metre planar)
     *   - Wall-ish vertex (|n.z| <= 0.5):  u=x*n.y - y*n.x (dot with wall
     *     tangent (n.y, -n.x)), v=z (absolute ENU z in metres).
     * Both in metres. Matches the terrain / linear world-metre UV
     * convention. */
    float *uvs = (float *)malloc((size_t)eb.nv * 2 * sizeof(float));
    if (!uvs) {
        osmmesh_emit_buf_free(&eb);
        return OSMMESH_BUILDING_ERR_OOM;
    }
    for (uint32_t i = 0; i < eb.nv; ++i) {
        float x  = eb.positions[3 * i + 0];
        float y  = eb.positions[3 * i + 1];
        float z  = eb.positions[3 * i + 2];
        float nx = eb.normals  [3 * i + 0];
        float ny = eb.normals  [3 * i + 1];
        float nz = eb.normals  [3 * i + 2];
        if (nz > 0.5f || nz < -0.5f) {
            uvs[2 * i + 0] = x; uvs[2 * i + 1] = y;
        } else {
            uvs[2 * i + 0] = x * ny - y * nx;
            uvs[2 * i + 1] = z;
        }
    }

    out_mesh->positions   = eb.positions;
    out_mesh->normals     = eb.normals;
    out_mesh->uvs         = uvs;
    out_mesh->indices     = eb.indices;
    out_mesh->n_vertices  = eb.nv;
    out_mesh->n_triangles = eb.nt;
    /* Ownership transferred to out_mesh — zero the emit buf handles. */
    eb.positions = NULL;
    eb.normals   = NULL;
    eb.indices   = NULL;
    osmmesh_emit_buf_free(&eb);

    if (out_info) {
        out_info->roof_shape         = dec.shape;
        out_info->wall_height_m      = dec.wall_h;
        out_info->roof_height_m      = dec.roof_h;
        out_info->footprint_area_m2  = fp.area_abs;
        out_info->obb_long_m         = fp.obb_long;
        out_info->obb_short_m        = fp.obb_short;
        out_info->footprint_vertices = fp.n;
        out_info->osm_id             = feature->id;
        out_info->is_rectangular     = fp.is_rectangular;
        out_info->is_regular         = fp.is_regular;
    }
    return OSMMESH_BUILDING_OK;
}
