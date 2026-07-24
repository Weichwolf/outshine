/* libosmmesh/src/pmtiles.c
 *
 * PMTiles v3 reader. See osmmesh/pmtiles.h for API contract.
 *
 * Parsing rules (strict):
 *   - magic must be "PMTiles", version must be 3.
 *   - internal_compression and tile_compression must both be 1 (none).
 *   - varint overflow (>10 bytes, or shift > 63) => ERR_CORRUPT.
 *   - directory arithmetic that exceeds the loaded buffer => ERR_CORRUPT.
 *   - a tile coord without matching entry => ERR_NOT_FOUND (no log).
 */

#include "osmmesh/pmtiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Internal types
 * ====================================================================== */

/* Directory entry (decoded). run_length == 0 means this entry points at a
 * leaf directory rather than a tile. */
typedef struct {
    uint64_t tile_id;
    uint64_t offset;      /* relative to tile_data_offset (tile) or
                             leaf_dirs_offset (leaf dir) */
    uint32_t length;
    uint32_t run_length;
} pm_entry;

struct osmmesh_pmtiles {
    osmmesh_pmtiles_io      io;
    int                     owns_io;      /* 0=borrowed, 1=file, 2=memory */
    osmmesh_pmtiles_header  hdr;
    pm_entry               *root;
    size_t                  root_n;
};

/* ========================================================================
 * Varint + little-endian readers (bounded)
 * ====================================================================== */

/* Read a protobuf-style unsigned varint from buf[pos..n); advance *pos.
 * Returns 0 on success, nonzero on overflow or truncation. */
static int rd_varint(const uint8_t *buf, size_t n, size_t *pos, uint64_t *out) {
    uint64_t v = 0;
    unsigned shift = 0;
    size_t p = *pos;
    for (int i = 0; i < 10; i++) {
        if (p >= n) return -1;
        uint8_t b = buf[p++];
        v |= (uint64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            *pos = p;
            *out = v;
            return 0;
        }
        shift += 7;
        if (shift > 63) return -1;
    }
    /* >10 bytes with continuation bit set => overflow */
    return -1;
}

static uint32_t rd_u32le(const uint8_t *p) {
    return  (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint64_t rd_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* ========================================================================
 * Hilbert curve <-> (z,x,y)
 *
 * Straight port of the PMTiles v3 spec pseudocode. xy2d is synchronized
 * with fixture_writer.c's hilbert_xy2d and must stay that way.
 * ====================================================================== */

static uint64_t hilbert_xy2d(uint8_t z, uint32_t x, uint32_t y) {
    uint64_t n = (uint64_t)1 << z;
    uint64_t rx, ry, d = 0;
    uint64_t xx = x, yy = y;
    for (uint64_t s = n / 2; s > 0; s /= 2) {
        rx = (xx & s) ? 1 : 0;
        ry = (yy & s) ? 1 : 0;
        d += s * s * ((3 * rx) ^ ry);
        if (ry == 0) {
            if (rx == 1) {
                xx = s - 1 - xx;
                yy = s - 1 - yy;
            }
            uint64_t t = xx; xx = yy; yy = t;
        }
    }
    return d;
}

static void hilbert_d2xy(uint8_t z, uint64_t d,
                          uint32_t *out_x, uint32_t *out_y) {
    uint64_t n = (uint64_t)1 << z;
    uint64_t rx, ry, t = d;
    uint64_t xx = 0, yy = 0;
    for (uint64_t s = 1; s < n; s *= 2) {
        rx = 1 & (t / 2);
        ry = 1 & (t ^ rx);
        if (ry == 0) {
            if (rx == 1) {
                xx = s - 1 - xx;
                yy = s - 1 - yy;
            }
            uint64_t tmp = xx; xx = yy; yy = tmp;
        }
        xx += s * rx;
        yy += s * ry;
        t /= 4;
    }
    *out_x = (uint32_t)xx;
    *out_y = (uint32_t)yy;
}

/* base(z) = sum_{i=0..z-1} 4^i = (4^z - 1) / 3 */
static uint64_t zoom_base(uint8_t z) {
    uint64_t acc = 0, pow4 = 1;
    for (uint8_t i = 0; i < z; i++) { acc += pow4; pow4 *= 4; }
    return acc;
}

uint64_t osmmesh_pmtiles_tile_id(uint8_t z, uint32_t x, uint32_t y) {
    return zoom_base(z) + hilbert_xy2d(z, x, y);
}

void osmmesh_pmtiles_tile_xy(uint64_t tile_id,
                              uint8_t *z_out, uint32_t *x_out, uint32_t *y_out) {
    /* Find z such that base(z) <= tile_id < base(z+1). Iterate rather than
     * solve closed-form; z is bounded by 30ish in practice. */
    uint8_t z = 0;
    uint64_t acc = 0, pow4 = 1;
    while (acc + pow4 <= tile_id) {
        acc += pow4;
        pow4 *= 4;
        z++;
        if (z > 31) break;  /* guard: PMTiles max zoom is 27 in practice */
    }
    hilbert_d2xy(z, tile_id - acc, x_out, y_out);
    *z_out = z;
}

/* ========================================================================
 * Directory decode
 *
 * Serialized form (per spec):
 *   varint num_entries
 *   num_entries varints of tile_id deltas (absolute tile_id = cumulative sum)
 *   num_entries varints of run_length
 *   num_entries varints of length
 *   num_entries varints of offset, with sentinel 0 meaning
 *     "prev_entry.offset + prev_entry.length" (only valid for i > 0;
 *     for i == 0 we treat 0 as literal "0", matching protomaps reference).
 * ====================================================================== */

static int decode_directory(const uint8_t *buf, size_t n,
                             pm_entry **out, size_t *out_n) {
    *out = NULL;
    *out_n = 0;

    size_t pos = 0;
    uint64_t n_entries = 0;
    if (rd_varint(buf, n, &pos, &n_entries) != 0) return OSMMESH_PMTILES_ERR_CORRUPT;
    if (n_entries == 0) {
        /* valid empty directory */
        return OSMMESH_PMTILES_OK;
    }
    /* Sanity cap: a directory with billions of entries is corrupt. */
    if (n_entries > (1ULL << 30)) return OSMMESH_PMTILES_ERR_CORRUPT;

    pm_entry *e = (pm_entry *)calloc((size_t)n_entries, sizeof(pm_entry));
    if (!e) return OSMMESH_PMTILES_ERR_OOM;

    /* tile_id deltas */
    uint64_t prev_id = 0;
    for (uint64_t i = 0; i < n_entries; i++) {
        uint64_t d;
        if (rd_varint(buf, n, &pos, &d) != 0) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        prev_id += d;
        e[i].tile_id = prev_id;
    }
    /* run_length */
    for (uint64_t i = 0; i < n_entries; i++) {
        uint64_t v;
        if (rd_varint(buf, n, &pos, &v) != 0) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        if (v > 0xFFFFFFFFu) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        e[i].run_length = (uint32_t)v;
    }
    /* length */
    for (uint64_t i = 0; i < n_entries; i++) {
        uint64_t v;
        if (rd_varint(buf, n, &pos, &v) != 0) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        if (v > 0xFFFFFFFFu) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        e[i].length = (uint32_t)v;
    }
    /* offset */
    for (uint64_t i = 0; i < n_entries; i++) {
        uint64_t v;
        if (rd_varint(buf, n, &pos, &v) != 0) { free(e); return OSMMESH_PMTILES_ERR_CORRUPT; }
        if (v == 0 && i > 0) {
            e[i].offset = e[i - 1].offset + (uint64_t)e[i - 1].length;
        } else if (v == 0) {
            e[i].offset = 0;
        } else {
            e[i].offset = v - 1;
        }
    }

    *out = e;
    *out_n = (size_t)n_entries;
    return OSMMESH_PMTILES_OK;
}

/* Binary-search a sorted entry array for a tile_id, honoring run_length.
 * Returns index of covering entry, or -1 if not found. Entries with
 * run_length == 0 (leaf-dir pointers) are treated as covering exactly
 * tile_ids >= entry.tile_id up to the next entry's tile_id; for lookup
 * we locate the greatest entry with tile_id <= target, and then the
 * caller checks the coverage semantics. */
static int64_t find_entry(const pm_entry *e, size_t n, uint64_t target) {
    if (n == 0) return -1;
    size_t lo = 0, hi = n;  /* invariant: answer in [lo, hi) */
    /* Upper-bound search: largest i with e[i].tile_id <= target */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (e[mid].tile_id <= target) lo = mid + 1;
        else                          hi = mid;
    }
    if (lo == 0) return -1;
    return (int64_t)(lo - 1);
}

/* ========================================================================
 * Header parse
 * ====================================================================== */

static int parse_header(const uint8_t h[127], osmmesh_pmtiles_header *out,
                         const char *path_for_log) {
    if (memcmp(h, "PMTiles", 7) != 0) {
        fprintf(stderr, "pmtiles: %s: bad magic\n", path_for_log ? path_for_log : "<io>");
        return OSMMESH_PMTILES_ERR_MAGIC;
    }
    if (h[7] != 3) {
        fprintf(stderr, "pmtiles: %s: unsupported version %u (need 3)\n",
                path_for_log ? path_for_log : "<io>", (unsigned)h[7]);
        return OSMMESH_PMTILES_ERR_VERSION;
    }

    out->version             = h[7];
    out->root_dir_offset     = rd_u64le(h +  8);
    out->root_dir_length     = rd_u64le(h + 16);
    out->metadata_offset     = rd_u64le(h + 24);
    out->metadata_length     = rd_u64le(h + 32);
    out->leaf_dirs_offset    = rd_u64le(h + 40);
    out->leaf_dirs_length    = rd_u64le(h + 48);
    out->tile_data_offset    = rd_u64le(h + 56);
    out->tile_data_length    = rd_u64le(h + 64);
    out->addressed_tiles     = rd_u64le(h + 72);
    out->tile_entries        = rd_u64le(h + 80);
    out->tile_contents       = rd_u64le(h + 88);
    out->clustered           = h[96];
    out->internal_compression = h[97];
    out->tile_compression    = h[98];
    out->tile_type           = h[99];
    out->min_zoom            = h[100];
    out->max_zoom            = h[101];

    int32_t min_lon_e7 = (int32_t)rd_u32le(h + 102);
    int32_t min_lat_e7 = (int32_t)rd_u32le(h + 106);
    int32_t max_lon_e7 = (int32_t)rd_u32le(h + 110);
    int32_t max_lat_e7 = (int32_t)rd_u32le(h + 114);
    out->min_lon = (double)min_lon_e7 / 1.0e7;
    out->min_lat = (double)min_lat_e7 / 1.0e7;
    out->max_lon = (double)max_lon_e7 / 1.0e7;
    out->max_lat = (double)max_lat_e7 / 1.0e7;

    out->center_zoom = h[118];
    int32_t ctr_lon_e7 = (int32_t)rd_u32le(h + 119);
    int32_t ctr_lat_e7 = (int32_t)rd_u32le(h + 123);
    out->center_lon = (double)ctr_lon_e7 / 1.0e7;
    out->center_lat = (double)ctr_lat_e7 / 1.0e7;

    if (out->internal_compression != 1) {
        fprintf(stderr,
            "pmtiles: %s: unsupported internal_compression=%u (need 1=none)\n",
            path_for_log ? path_for_log : "<io>", (unsigned)out->internal_compression);
        return OSMMESH_PMTILES_ERR_COMPRESSION;
    }
    if (out->tile_compression != 1) {
        fprintf(stderr,
            "pmtiles: %s: unsupported tile_compression=%u (need 1=none)\n",
            path_for_log ? path_for_log : "<io>", (unsigned)out->tile_compression);
        return OSMMESH_PMTILES_ERR_COMPRESSION;
    }
    return OSMMESH_PMTILES_OK;
}

/* ========================================================================
 * Public open/close
 * ====================================================================== */

/* From pmtiles_io.c */
extern int osmmesh_pmtiles__file_open(osmmesh_pmtiles_io *out, const char *path);
extern void osmmesh_pmtiles__file_close(osmmesh_pmtiles_io *io);

/* From pmtiles_io_memory.c */
extern int osmmesh_pmtiles__mem_open(osmmesh_pmtiles_io *out,
                                      const uint8_t *data, size_t len);
extern void osmmesh_pmtiles__mem_close(osmmesh_pmtiles_io *io);

/* Which close routine to dispatch on when owns_io is set. The file and
 * memory backends do not share cleanup code, so we tag the reader. */
enum { PM_OWNS_NONE = 0, PM_OWNS_FILE = 1, PM_OWNS_MEM = 2 };

int osmmesh_pmtiles_open_io(osmmesh_pmtiles **out, const osmmesh_pmtiles_io *io) {
    if (!out || !io || !io->read) return OSMMESH_PMTILES_ERR_ARG;

    osmmesh_pmtiles *p = (osmmesh_pmtiles *)calloc(1, sizeof(*p));
    if (!p) return OSMMESH_PMTILES_ERR_OOM;
    p->io = *io;
    p->owns_io = PM_OWNS_NONE;

    uint8_t hdr[127];
    if (p->io.read(p->io.ctx, 0, sizeof(hdr), hdr) != 0) {
        fprintf(stderr, "pmtiles: io error reading header\n");
        free(p);
        return OSMMESH_PMTILES_ERR_IO;
    }
    int rc = parse_header(hdr, &p->hdr, NULL);
    if (rc != OSMMESH_PMTILES_OK) { free(p); return rc; }

    /* Load root directory. */
    if (p->hdr.root_dir_length == 0) {
        /* empty archive is valid but useless */
        *out = p;
        return OSMMESH_PMTILES_OK;
    }
    if (p->hdr.root_dir_length > (1ULL << 28)) {
        fprintf(stderr, "pmtiles: root_dir_length=%llu looks corrupt\n",
                (unsigned long long)p->hdr.root_dir_length);
        free(p);
        return OSMMESH_PMTILES_ERR_CORRUPT;
    }
    uint8_t *rbuf = (uint8_t *)malloc((size_t)p->hdr.root_dir_length);
    if (!rbuf) { free(p); return OSMMESH_PMTILES_ERR_OOM; }
    if (p->io.read(p->io.ctx, p->hdr.root_dir_offset,
                    (size_t)p->hdr.root_dir_length, rbuf) != 0) {
        fprintf(stderr, "pmtiles: io error reading root directory\n");
        free(rbuf); free(p);
        return OSMMESH_PMTILES_ERR_IO;
    }
    rc = decode_directory(rbuf, (size_t)p->hdr.root_dir_length,
                           &p->root, &p->root_n);
    free(rbuf);
    if (rc != OSMMESH_PMTILES_OK) {
        fprintf(stderr, "pmtiles: root directory decode failed (%d)\n", rc);
        free(p);
        return rc;
    }

    *out = p;
    return OSMMESH_PMTILES_OK;
}

int osmmesh_pmtiles_open_file(osmmesh_pmtiles **out, const char *path) {
    if (!out || !path) return OSMMESH_PMTILES_ERR_ARG;
    osmmesh_pmtiles_io io;
    int rc = osmmesh_pmtiles__file_open(&io, path);
    if (rc != 0) {
        fprintf(stderr, "pmtiles: cannot open %s\n", path);
        return OSMMESH_PMTILES_ERR_IO;
    }
    rc = osmmesh_pmtiles_open_io(out, &io);
    if (rc != OSMMESH_PMTILES_OK) {
        osmmesh_pmtiles__file_close(&io);
        return rc;
    }
    (*out)->owns_io = PM_OWNS_FILE;
    return OSMMESH_PMTILES_OK;
}

int osmmesh_pmtiles_open_memory(osmmesh_pmtiles **out,
                                 const uint8_t *data, size_t len) {
    if (!out || !data) return OSMMESH_PMTILES_ERR_ARG;
    osmmesh_pmtiles_io io;
    int rc = osmmesh_pmtiles__mem_open(&io, data, len);
    if (rc != 0) {
        fprintf(stderr, "pmtiles: cannot init memory backend\n");
        return OSMMESH_PMTILES_ERR_OOM;
    }
    rc = osmmesh_pmtiles_open_io(out, &io);
    if (rc != OSMMESH_PMTILES_OK) {
        osmmesh_pmtiles__mem_close(&io);
        return rc;
    }
    (*out)->owns_io = PM_OWNS_MEM;
    return OSMMESH_PMTILES_OK;
}

void osmmesh_pmtiles_close(osmmesh_pmtiles *p) {
    if (!p) return;
    if (p->owns_io == PM_OWNS_FILE) osmmesh_pmtiles__file_close(&p->io);
    else if (p->owns_io == PM_OWNS_MEM) osmmesh_pmtiles__mem_close(&p->io);
    free(p->root);
    free(p);
}

const osmmesh_pmtiles_header *osmmesh_pmtiles_header_get(const osmmesh_pmtiles *p) {
    return p ? &p->hdr : NULL;
}

/* ========================================================================
 * Fetch
 * ====================================================================== */

/* Load a leaf directory from the archive. The root entry's offset/length
 * are relative to hdr.leaf_dirs_offset. Caller frees *out. */
static int load_leaf(osmmesh_pmtiles *p,
                      uint64_t leaf_off_rel, uint32_t leaf_len,
                      pm_entry **out, size_t *out_n) {
    *out = NULL; *out_n = 0;
    if (leaf_len == 0) return OSMMESH_PMTILES_ERR_CORRUPT;
    /* Bounds check against leaf_dirs span if the header declares one. */
    if (p->hdr.leaf_dirs_length > 0) {
        if ((uint64_t)leaf_off_rel + leaf_len > p->hdr.leaf_dirs_length) {
            return OSMMESH_PMTILES_ERR_CORRUPT;
        }
    }
    uint8_t *buf = (uint8_t *)malloc(leaf_len);
    if (!buf) return OSMMESH_PMTILES_ERR_OOM;
    uint64_t abs = p->hdr.leaf_dirs_offset + leaf_off_rel;
    if (p->io.read(p->io.ctx, abs, leaf_len, buf) != 0) {
        free(buf);
        fprintf(stderr, "pmtiles: io error reading leaf directory\n");
        return OSMMESH_PMTILES_ERR_IO;
    }
    int rc = decode_directory(buf, leaf_len, out, out_n);
    free(buf);
    return rc;
}

int osmmesh_pmtiles_fetch(osmmesh_pmtiles *p,
                           uint8_t z, uint32_t x, uint32_t y,
                           uint8_t **out_data, size_t *out_len) {
    if (!p || !out_data || !out_len) return OSMMESH_PMTILES_ERR_ARG;
    *out_data = NULL;
    *out_len = 0;

    uint64_t target = osmmesh_pmtiles_tile_id(z, x, y);

    const pm_entry *dir = p->root;
    size_t dir_n = p->root_n;
    pm_entry *leaf = NULL;

    /* Follow leaf directories up to a bounded depth (spec permits hierarchy
     * but in practice only one or two levels; cap at 3 to avoid pathological
     * archives). */
    for (int depth = 0; depth < 3; depth++) {
        int64_t idx = find_entry(dir, dir_n, target);
        if (idx < 0) {
            free(leaf);
            return OSMMESH_PMTILES_ERR_NOT_FOUND;
        }
        const pm_entry *e = &dir[idx];

        if (e->run_length == 0) {
            /* Leaf-directory pointer. Recurse. */
            pm_entry *new_leaf = NULL;
            size_t new_n = 0;
            int rc = load_leaf(p, e->offset, e->length, &new_leaf, &new_n);
            if (rc != OSMMESH_PMTILES_OK) {
                free(leaf);
                return rc;
            }
            free(leaf);
            leaf = new_leaf;
            dir = leaf;
            dir_n = new_n;
            continue;
        }

        /* Leaf entry: check run_length coverage. */
        if (target >= e->tile_id + (uint64_t)e->run_length) {
            free(leaf);
            return OSMMESH_PMTILES_ERR_NOT_FOUND;
        }

        uint32_t len = e->length;
        uint64_t off_abs = p->hdr.tile_data_offset + e->offset;
        if (p->hdr.tile_data_length > 0) {
            if (e->offset + (uint64_t)len > p->hdr.tile_data_length) {
                free(leaf);
                return OSMMESH_PMTILES_ERR_CORRUPT;
            }
        }
        uint8_t *buf = (uint8_t *)malloc(len ? len : 1);
        if (!buf) { free(leaf); return OSMMESH_PMTILES_ERR_OOM; }
        if (len > 0 && p->io.read(p->io.ctx, off_abs, len, buf) != 0) {
            free(buf);
            free(leaf);
            fprintf(stderr, "pmtiles: io error reading tile payload\n");
            return OSMMESH_PMTILES_ERR_IO;
        }
        free(leaf);
        *out_data = buf;
        *out_len = len;
        return OSMMESH_PMTILES_OK;
    }

    free(leaf);
    return OSMMESH_PMTILES_ERR_CORRUPT;  /* depth cap exceeded */
}

void osmmesh_pmtiles_free_tile(uint8_t *data) {
    free(data);
}

/* ========================================================================
 * Enumerate
 * ====================================================================== */

/* Recursive walker: emits every (z,x,y, offset, length) for a directory.
 * Returns first nonzero cb result, or 0 on clean traversal. */
static int walk_dir(osmmesh_pmtiles *p,
                     const pm_entry *dir, size_t dir_n,
                     int (*cb)(void *, uint8_t, uint32_t, uint32_t,
                               uint64_t, uint32_t),
                     void *user) {
    for (size_t i = 0; i < dir_n; i++) {
        const pm_entry *e = &dir[i];
        if (e->run_length == 0) {
            pm_entry *leaf = NULL;
            size_t leaf_n = 0;
            int rc = load_leaf(p, e->offset, e->length, &leaf, &leaf_n);
            if (rc != OSMMESH_PMTILES_OK) return rc;
            int r = walk_dir(p, leaf, leaf_n, cb, user);
            free(leaf);
            if (r != 0) return r;
            continue;
        }
        for (uint32_t k = 0; k < e->run_length; k++) {
            uint8_t  z;
            uint32_t x, y;
            osmmesh_pmtiles_tile_xy(e->tile_id + k, &z, &x, &y);
            uint64_t abs_off = p->hdr.tile_data_offset + e->offset;
            int r = cb(user, z, x, y, abs_off, e->length);
            if (r != 0) return r;
        }
    }
    return 0;
}

int osmmesh_pmtiles_enumerate(osmmesh_pmtiles *p,
    int (*cb)(void *user, uint8_t z, uint32_t x, uint32_t y,
              uint64_t offset, uint32_t length),
    void *user) {
    if (!p || !cb) return OSMMESH_PMTILES_ERR_ARG;
    return walk_dir(p, p->root, p->root_n, cb, user);
}
