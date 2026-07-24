/* libosmmesh/include/osmmesh/mvt.h
 *
 * MVT 2.1 (Mapbox Vector Tile) decoder.
 *
 * Scope:
 *   - Decode one tile payload (raw bytes from PMTiles) into layers/features/
 *     geometries/tags.
 *   - Geometry commands: MoveTo (1), LineTo (2), ClosePath (7).
 *   - Value kinds: string, float, double, int64, uint64, sint64, bool.
 *
 * Restrictions:
 *   - Caller owns the lifetime of the input bytes only during the decode call;
 *     the returned tile tree holds its own copies of all strings and arrays
 *     in a single bump arena, so the input buffer can be freed immediately
 *     after osmmesh_mvt_decode returns.
 *   - Unknown protobuf fields are tolerated (forward-compatible with future
 *     spec extensions); unknown wire types (groups: 3/4) are treated as
 *     corrupt.
 *
 * Spec: https://github.com/mapbox/vector-tile-spec/tree/master/2.1
 */

#ifndef OSMMESH_MVT_H
#define OSMMESH_MVT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct osmmesh_mvt_tile  osmmesh_mvt_tile;
typedef struct osmmesh_mvt_layer osmmesh_mvt_layer;

typedef enum {
    OSMMESH_MVT_GEOM_UNKNOWN    = 0,
    OSMMESH_MVT_GEOM_POINT      = 1,
    OSMMESH_MVT_GEOM_LINESTRING = 2,
    OSMMESH_MVT_GEOM_POLYGON    = 3
} osmmesh_mvt_geom_type;

typedef enum {
    OSMMESH_MVT_VAL_NONE,
    OSMMESH_MVT_VAL_STRING,
    OSMMESH_MVT_VAL_FLOAT,
    OSMMESH_MVT_VAL_DOUBLE,
    OSMMESH_MVT_VAL_INT,
    OSMMESH_MVT_VAL_UINT,
    OSMMESH_MVT_VAL_SINT,
    OSMMESH_MVT_VAL_BOOL
} osmmesh_mvt_val_type;

typedef struct {
    osmmesh_mvt_val_type type;
    union {
        const char *s;
        float       f;
        double      d;
        int64_t     i;
        uint64_t    u;
        int64_t     z;
        int         b;
    } v;
} osmmesh_mvt_value;

/* Coord in local tile space. Layer extent bounds the value range; caller is
 * responsible for any world-space conversion. MVT 2.1 technically permits
 * negative coords (vertices outside the tile box used for clipping join); we
 * store raw int32 to preserve that. */
typedef struct { int32_t x, y; } osmmesh_mvt_coord;

typedef struct {
    uint64_t                 id;
    osmmesh_mvt_geom_type    geom_type;
    const osmmesh_mvt_coord *coords;
    size_t                   n_coords;
    /* ring_offsets has length n_rings+1; [i] is the start coord index of ring i,
     * [n_rings] = n_coords. For POINT features n_rings == 0 and ring_offsets
     * is NULL; the caller uses n_coords directly. */
    const uint32_t          *ring_offsets;
    size_t                   n_rings;
    /* tag_pairs is a flat [k0, v0, k1, v1, ...] indirection stream. Length is
     * n_tag_pairs * 2 entries in the raw stream, but we expose "pair count"
     * for convenience. Look up names/values via the helpers below. */
    const uint32_t          *tag_pairs;
    size_t                   n_tag_pairs;
} osmmesh_mvt_feature;

/* Decode a raw MVT tile payload into a heap-owned tree. Returns OSMMESH_MVT_OK
 * on success and fills *out; returns a negative error otherwise and *out is
 * left as NULL. */
int  osmmesh_mvt_decode(const uint8_t *data, size_t len, osmmesh_mvt_tile **out);
void osmmesh_mvt_free  (osmmesh_mvt_tile *tile);

size_t                    osmmesh_mvt_num_layers(const osmmesh_mvt_tile *t);
const osmmesh_mvt_layer  *osmmesh_mvt_layer_at  (const osmmesh_mvt_tile *t, size_t idx);

const char *osmmesh_mvt_layer_name   (const osmmesh_mvt_layer *l);
uint32_t    osmmesh_mvt_layer_extent (const osmmesh_mvt_layer *l);
uint32_t    osmmesh_mvt_layer_version(const osmmesh_mvt_layer *l);

size_t                     osmmesh_mvt_num_features(const osmmesh_mvt_layer *l);
const osmmesh_mvt_feature *osmmesh_mvt_feature_at  (const osmmesh_mvt_layer *l,
                                                     size_t idx);

/* Tag resolution: pair_idx in [0, n_tag_pairs). Returns NULL / nonzero on
 * out-of-range / missing. */
const char *osmmesh_mvt_feature_tag_key  (const osmmesh_mvt_layer   *l,
                                           const osmmesh_mvt_feature *f,
                                           size_t                     pair_idx);
int         osmmesh_mvt_feature_tag_value(const osmmesh_mvt_layer   *l,
                                           const osmmesh_mvt_feature *f,
                                           size_t                     pair_idx,
                                           osmmesh_mvt_value         *out);

/* Convenience: find a tag value by key name. Returns 0 on hit (fills *out),
 * nonzero if the feature has no such tag. */
int osmmesh_mvt_feature_get_tag(const osmmesh_mvt_layer   *l,
                                 const osmmesh_mvt_feature *f,
                                 const char                *key,
                                 osmmesh_mvt_value         *out);

/* Error codes. */
#define OSMMESH_MVT_OK                 0
#define OSMMESH_MVT_ERR_CORRUPT       -1
#define OSMMESH_MVT_ERR_OOM           -2
#define OSMMESH_MVT_ERR_ARG           -3
#define OSMMESH_MVT_ERR_UNSUPPORTED   -4

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_MVT_H */
