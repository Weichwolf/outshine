/* libosmmesh/include/osmmesh/linear.h
 *
 * Linear-feature mesh generator (T7, MVP). Turns one MVT linestring feature
 * (or multi-linestring) into a flat ribbon mesh in ENU meters. Classifies
 * the feature against a small fixed taxonomy (motorway..path, rail/tram/
 * subway) and picks a default width from a class table; callers may override
 * the width per-feature.
 *
 * Scope (MVP):
 *   - Flat ribbons: all vertices at z=0, normal = (0,0,1). Terrain drape is
 *     deferred to a later phase.
 *   - Single-width band per feature. No multi-material cross-sections, no
 *     lane markings, no kerbs.
 *   - Mitre joins at interior knees, with a mitre-limit fallback that emits
 *     a bevel (two coincident corner pairs + a filler triangle) when the
 *     mitre extension would explode at near-180-degree turns.
 *   - Multi-linestring features (MVT n_rings > 1 on LINESTRING geom) produce
 *     multiple disjoint ribbons concatenated into one mesh.
 *
 * Winding / normal (derivation — do NOT guess):
 *   Polyline edge direction d_i = normalize(p_{i+1} - p_i) in ENU (e,n).
 *   "Left" perpendicular in the xy plane, rotated +90 deg CCW around +Z:
 *       n_i = perp_left(d_i) = (-d_i.y, d_i.x).
 *   Emitted vertex pair at spine point p_i: left = p_i + n * w/2,
 *                                           right = p_i - n * w/2.
 *   Per segment, the quad is (left_i, right_i) -> (left_{i+1}, right_{i+1}).
 *   For the quad to have an upward-facing normal (+Z) under a right-handed
 *   basis (E right, N up, U up), the winding is:
 *       tri 0: (left_i, right_i, right_{i+1})
 *       tri 1: (left_i, right_{i+1}, left_{i+1})
 *   Check: (right_i - left_i) = -w * n_i, (right_{i+1} - left_i) spans
 *   forward along d, so cross((right_i - left_i), (right_{i+1} - left_i))
 *   has +z component (algebra in linear_ribbon.c). Vertex normals written
 *   to the buffer are (0,0,1) and face normals agree.
 */

#ifndef OSMMESH_LINEAR_H
#define OSMMESH_LINEAR_H

#include <stdint.h>

#include "osmmesh/geo.h"
#include "osmmesh/mesh.h"
#include "osmmesh/mvt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OSMMESH_LINEAR_UNKNOWN     = 0,
    OSMMESH_LINEAR_MOTORWAY,
    OSMMESH_LINEAR_TRUNK,
    OSMMESH_LINEAR_PRIMARY,
    OSMMESH_LINEAR_SECONDARY,
    OSMMESH_LINEAR_TERTIARY,
    OSMMESH_LINEAR_RESIDENTIAL,
    OSMMESH_LINEAR_SERVICE,
    OSMMESH_LINEAR_FOOTWAY,
    OSMMESH_LINEAR_CYCLEWAY,
    OSMMESH_LINEAR_PATH,
    OSMMESH_LINEAR_RAIL,
    OSMMESH_LINEAR_TRAM,
    OSMMESH_LINEAR_SUBWAY,
    /* Waterways (Shortbread `water_lines` layer / OSM `waterway=*`).
     * Width is the visible water surface; we do NOT excavate the terrain
     * underneath in MVP — the ribbon drapes on the heightgrid like a road. */
    OSMMESH_LINEAR_RIVER,
    OSMMESH_LINEAR_CANAL,
    OSMMESH_LINEAR_STREAM,
    OSMMESH_LINEAR_DRAIN,
    OSMMESH_LINEAR_DITCH,
    OSMMESH_LINEAR_COUNT
} osmmesh_linear_kind;

typedef struct {
    /* If >0, override width; else use class-table. */
    float width_override_m;
    /* Skip features shorter than this (0 = keep all). Useful to filter noise. */
    float min_length_m;
    /* Mitre limit: ratio of mitre length to half-width beyond which we clip
     * to a bevel (two coincident-origin triangles). Typical: 4.0. */
    float mitre_limit;
    /* If nonzero, emit texture coordinates along the length in meters (u=s,
     * v in [0,1] across width). Zero = no UVs. */
    int   emit_uvs;
} osmmesh_linear_opts;

typedef struct {
    osmmesh_linear_kind kind;
    float               width_m;
    float               length_m;     /* total, sum of sub-strings */
    uint32_t            n_substrings; /* for multi-line features */
    uint32_t            n_segments;   /* total edge count */
} osmmesh_linear_info;

/* Build a ribbon mesh from one MVT LINESTRING feature. Multi-linestring
 * features (multiple MoveTo commands) produce multiple disjoint ribbons
 * concatenated into one mesh. Uses shortbread `kind` tag or falls back to
 * OMT `class`/`highway`. */
int osmmesh_linear_build(const osmmesh_mvt_layer *layer,
                          const osmmesh_mvt_feature *feature,
                          const osmmesh_tile_enu_map *map,
                          const osmmesh_linear_opts *opts,      /* NULL = defaults */
                          osmmesh_mesh *out_mesh,
                          osmmesh_linear_info *out_info);       /* NULL ok */

/* Classification from tags. Exposed for tests + late binding in T8. */
osmmesh_linear_kind osmmesh_linear_classify(const osmmesh_mvt_layer *layer,
                                              const osmmesh_mvt_feature *feature);

/* Default width per class (m). Extern-const table could be useful; we keep
 * it as a function so the table lives in a single TU. */
float osmmesh_linear_default_width(osmmesh_linear_kind kind);

/* Class priority for overlap arbitration during terrain planishing.
 * Higher value wins. Opaque encoding; callers should only rely on ordering,
 * not exact numbers. Returns a negative value to signal "do not planish"
 * (subway and other underground classes). */
int osmmesh_linear_class_priority(osmmesh_linear_kind kind);

#define OSMMESH_LINEAR_OK         0
#define OSMMESH_LINEAR_ERR_SKIP  -1   /* <2 vertices, zero length, classification UNKNOWN */
#define OSMMESH_LINEAR_ERR_ARG   -2
#define OSMMESH_LINEAR_ERR_OOM   -3

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_LINEAR_H */
