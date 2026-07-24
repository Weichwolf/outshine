/* libosmmesh/include/osmmesh/pmtiles.h
 *
 * PMTiles v3 archive reader.
 *
 * Scope:
 *   - 127-byte header parse with strict magic/version/compression checks.
 *   - Root + leaf directory decode (5 delta-varint arrays per spec).
 *   - Hilbert tile-id mapping (z,x,y) <-> tile_id.
 *   - Pluggable IO backend; a file-backed default is provided.
 *
 * Restrictions (intentional for this phase):
 *   - internal_compression and tile_compression MUST both be 1 (none). Any
 *     other value is rejected with OSMMESH_PMTILES_ERR_COMPRESSION. All of
 *     our Shortbread and Terrarium PMTiles are built with --compress=none.
 *   - No MVT decoding: fetch returns raw tile bytes.
 *   - No HTTP: that ships in T10 as a second IO backend.
 *
 * Spec: https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md
 */

#ifndef OSMMESH_PMTILES_H
#define OSMMESH_PMTILES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct osmmesh_pmtiles osmmesh_pmtiles;

/* Pluggable IO backend. `read` is called with absolute byte offsets into the
 * PMTiles archive and must fill `buf[0..len)`. Return 0 on success, nonzero
 * on error. `ctx` is the user-supplied backend state. `size` is optional;
 * return 0 if unknown (the reader tolerates that, it just can't bounds-check
 * pre-read). */
typedef struct {
    int      (*read)(void *ctx, uint64_t offset, size_t len, void *buf);
    uint64_t (*size)(void *ctx);
    void      *ctx;
} osmmesh_pmtiles_io;

typedef struct {
    uint8_t  version;               /* 3 */
    uint8_t  clustered;
    uint8_t  internal_compression;  /* 1 = none (only accepted value) */
    uint8_t  tile_compression;      /* 1 = none */
    uint8_t  tile_type;             /* 1 = MVT */
    uint8_t  min_zoom, max_zoom;
    uint8_t  center_zoom;
    double   min_lat, min_lon, max_lat, max_lon;
    double   center_lat, center_lon;
    uint64_t addressed_tiles;       /* sum of run_lengths */
    uint64_t tile_entries;          /* number of directory entries */
    uint64_t tile_contents;         /* number of unique payloads */
    uint64_t root_dir_offset,  root_dir_length;
    uint64_t leaf_dirs_offset, leaf_dirs_length;
    uint64_t tile_data_offset, tile_data_length;
    uint64_t metadata_offset,  metadata_length;
} osmmesh_pmtiles_header;

/* Error codes. 0 = success, negative = error. NOT_FOUND is an expected
 * lookup outcome, not a hard failure, and is not logged. */
#define OSMMESH_PMTILES_OK                   0
#define OSMMESH_PMTILES_ERR_IO              -1
#define OSMMESH_PMTILES_ERR_MAGIC           -2
#define OSMMESH_PMTILES_ERR_VERSION         -3
#define OSMMESH_PMTILES_ERR_COMPRESSION     -4
#define OSMMESH_PMTILES_ERR_CORRUPT         -5
#define OSMMESH_PMTILES_ERR_NOT_FOUND       -6
#define OSMMESH_PMTILES_ERR_OOM             -7
#define OSMMESH_PMTILES_ERR_ARG             -8

/* Open an archive using a caller-supplied IO backend. The backend is
 * borrowed (not copied); keep it alive until osmmesh_pmtiles_close. */
int osmmesh_pmtiles_open_io(osmmesh_pmtiles **out, const osmmesh_pmtiles_io *io);

/* Open a file-backed archive. Internally constructs an IO backend over
 * fopen/fseek/fread and owns it; close with osmmesh_pmtiles_close. */
int osmmesh_pmtiles_open_file(osmmesh_pmtiles **out, const char *path);

/* Open an archive backed by an in-memory byte buffer. The reader borrows the
 * buffer (no copy); caller must keep it alive until osmmesh_pmtiles_close.
 * Intended for WASM where the browser fetches the whole .pmtiles via fetch()
 * into a single ArrayBuffer and hands the pointer to the reader. */
int osmmesh_pmtiles_open_memory(osmmesh_pmtiles **out,
                                 const uint8_t *data, size_t len);

void osmmesh_pmtiles_close(osmmesh_pmtiles *p);

/* Borrowed pointer; valid until close. */
const osmmesh_pmtiles_header *osmmesh_pmtiles_header_get(const osmmesh_pmtiles *p);

/* Fetch a single tile by (z,x,y). On success allocates *out_data (caller owns,
 * free via osmmesh_pmtiles_free_tile) and sets *out_len. Returns NOT_FOUND
 * when the coordinate has no entry; that is not an error log. */
int osmmesh_pmtiles_fetch(osmmesh_pmtiles *p,
                           uint8_t z, uint32_t x, uint32_t y,
                           uint8_t **out_data, size_t *out_len);
void osmmesh_pmtiles_free_tile(uint8_t *data);

/* Hilbert-curve tile_id mapping per PMTiles v3 spec.
 *   tile_id = sum_{i=0..z-1} 4^i + hilbert(z, x, y)
 * Inverse: recover z first by locating the base, then hilbert_d2xy. */
uint64_t osmmesh_pmtiles_tile_id(uint8_t z, uint32_t x, uint32_t y);
void     osmmesh_pmtiles_tile_xy(uint64_t tile_id,
                                  uint8_t *z, uint32_t *x, uint32_t *y);

/* Enumerate every addressed tile. For entries with run_length > 1 the
 * callback is invoked once per covered tile_id (each expanded to its own
 * (z,x,y)). For leaf-dir entries the leaf is loaded, expanded, released.
 * Returning nonzero from `cb` aborts enumeration and is returned verbatim. */
int osmmesh_pmtiles_enumerate(osmmesh_pmtiles *p,
    int (*cb)(void *user, uint8_t z, uint32_t x, uint32_t y,
              uint64_t offset, uint32_t length),
    void *user);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_PMTILES_H */
