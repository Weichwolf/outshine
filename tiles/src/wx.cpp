#include "wx.h"
#include "wxfmt.h"
#include "grib2.h"
#include "http.h"
#include "reply.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

/* Source product: the NOMADS grib_filter cut of the GFS 0.25 degree analysis (f000). The source
 * grid is a compile-time fact, not something read from the response -- the response is validated
 * against it, so a changed upstream product fails loudly instead of resizing our buffers.
 *
 * FB_WX_STEP is the one knob: the output grid takes every STEP-th source point on an exact
 * sub-lattice (STEP=2 -> 0.5 deg, 720x361). Sub-sampling, not averaging, and deliberately so --
 * every value we ship is then literally a GFS grid-point value rather than a number we invented,
 * which is also what makes a point check against an independent GFS consumer meaningful. At 0.25
 * deg the full field set is 33 MB, well past what a client wants to pull once per session; at
 * STEP=2 it is 8.3 MB, at STEP=4 2.1 MB. The blob header carries nx/ny/dlat/dlon, so a reader
 * never encodes this choice. */
#define FB_WX_SRC_NX 1440
#define FB_WX_SRC_NY 721
#ifndef FB_WX_STEP
#define FB_WX_STEP 2
#endif
static_assert(FB_WX_SRC_NX % FB_WX_STEP == 0, "FB_WX_STEP must divide the source grid width");
static_assert((FB_WX_SRC_NY - 1) % FB_WX_STEP == 0, "FB_WX_STEP must divide the source grid height");
#define FB_WX_NX (FB_WX_SRC_NX / FB_WX_STEP)
#define FB_WX_NY ((FB_WX_SRC_NY - 1) / FB_WX_STEP + 1)

#define FB_WX_RUN_PERIOD_S   21600L   /* GFS cycles at 00/06/12/18Z */
#define FB_WX_RUN_LATENCY_S  14400L   /* a cycle lands on NOMADS ~3.5-5 h after its analysis time */
#define FB_WX_LIVE_TRIES     4        /* cycles probed against NOMADS, newest first (24 h) */
#define FB_WX_DISK_TRIES     8        /* cycles looked for on disk when NOMADS is unreachable (48 h) */
#define FB_WX_RETRY_S        600L     /* floor on re-probing after a miss, so a gap can't hammer NOMADS */
#define FB_WX_MIN_TTL_S      300L
#define FB_WX_HTTP_TIMEOUT_S 120L
#define FB_WX_MAX_GRIB       (48u << 20)

typedef struct {
    uint8_t  var, lev_kind;
    uint16_t lev_value;
    uint8_t  cat, num, ltype;   /* GRIB2 parameter category/number and code table 4.5 surface */
    double   grib_level;        /* first fixed surface as GRIB states it: Pa for isobaric, m for AGL */
    uint8_t  bits;
    float    vmin, vmax;
    /* Quantisation ranges are FIXED, never derived from the data: identical input must produce
     * identical bytes across runs and restarts, and a client must be able to hard-code the meaning
     * of a raw value. Out-of-range saturates (never wraps). The GFS ranges they bracket are noted
     * per row; the wind window covers any jet stream with room to spare. */
    float    missing_at;        /* >= this is "no value" (0 = the field has none) */
} fb_wx_field;

static const fb_wx_field g_fields[] = {
    /* var               level                value  cat num ltype  grib_lev  bits    vmin      vmax  missing */
    { FB_WX_VAR_WIND_U, FB_WX_LEV_AGL,          10,   2,  2,  103,     10.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_V, FB_WX_LEV_AGL,          10,   2,  3,  103,     10.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_U, FB_WX_LEV_ISOBARIC,    850,   2,  2,  100,  85000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_V, FB_WX_LEV_ISOBARIC,    850,   2,  3,  100,  85000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_U, FB_WX_LEV_ISOBARIC,    700,   2,  2,  100,  70000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_V, FB_WX_LEV_ISOBARIC,    700,   2,  3,  100,  70000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_U, FB_WX_LEV_ISOBARIC,    500,   2,  2,  100,  50000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_V, FB_WX_LEV_ISOBARIC,    500,   2,  3,  100,  50000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_U, FB_WX_LEV_ISOBARIC,    250,   2,  2,  100,  25000.0,   16, -180.0f,  180.0f,     0.0f },
    { FB_WX_VAR_WIND_V, FB_WX_LEV_ISOBARIC,    250,   2,  3,  100,  25000.0,   16, -180.0f,  180.0f,     0.0f },
    /* geopotential height of each wind level, so a client places the profile in metres instead of
     * assuming a standard atmosphere. 8 bit with a tight per-level window: the observed global
     * spread of each surface is ~800-2100 m, the windows below add >=400 m of margin on each side
     * and still resolve 5-12 m. */
    { FB_WX_VAR_HEIGHT, FB_WX_LEV_ISOBARIC,    850,   3,  5,  100,  85000.0,    8,  600.0f, 1800.0f,     0.0f },
    { FB_WX_VAR_HEIGHT, FB_WX_LEV_ISOBARIC,    700,   3,  5,  100,  70000.0,    8, 2000.0f, 3500.0f,     0.0f },
    { FB_WX_VAR_HEIGHT, FB_WX_LEV_ISOBARIC,    500,   3,  5,  100,  50000.0,    8, 4300.0f, 6100.0f,     0.0f },
    { FB_WX_VAR_HEIGHT, FB_WX_LEV_ISOBARIC,    250,   3,  5,  100,  25000.0,    8, 8500.0f,11500.0f,     0.0f },
    /* GFS reports "no ceiling" as ~20000 m rather than a bitmap; that becomes a real missing flag
     * here, because a renderer must not draw a cloud base at 20 km. */
    { FB_WX_VAR_HEIGHT, FB_WX_LEV_CLOUD_CEIL,    0,   3,  5,  215,      0.0,   16,    0.0f,20000.0f, 19000.0f },
    { FB_WX_VAR_CLOUD,  FB_WX_LEV_ATMOSPHERE,    0,   6,  1,   10,      0.0,    8,    0.0f,  100.0f,     0.0f },
    { FB_WX_VAR_CLOUD,  FB_WX_LEV_CLOUD_LOW,     0,   6,  3,  214,      0.0,    8,    0.0f,  100.0f,     0.0f },
    { FB_WX_VAR_CLOUD,  FB_WX_LEV_CLOUD_MID,     0,   6,  4,  224,      0.0,    8,    0.0f,  100.0f,     0.0f },
    { FB_WX_VAR_CLOUD,  FB_WX_LEV_CLOUD_HIGH,    0,   6,  5,  234,      0.0,    8,    0.0f,  100.0f,     0.0f },
    /* GFS caps unlimited visibility at ~24.1 km; 16 bit keeps fog resolved to well under a metre,
     * which is the only part of the range anyone cares about. */
    { FB_WX_VAR_VIS,    FB_WX_LEV_SURFACE,       0,  19,  0,    1,      0.0,   16,    0.0f,24500.0f,     0.0f },
};
#define FB_WX_NFIELDS ((int)(sizeof g_fields / sizeof g_fields[0]))

/* The grib_filter CGI emits the CROSS PRODUCT of the selected variables and levels, so one request
 * for everything would also drag in TCDC on the four pressure levels and HGT at the surface -- ~5 MB
 * of records to throw away. Three requests fetch exactly the 20 records above. */
static const char *const g_groups[] = {
    "&var_UGRD=on&var_VGRD=on"
    "&lev_10_m_above_ground=on&lev_850_mb=on&lev_700_mb=on&lev_500_mb=on&lev_250_mb=on",
    "&var_HGT=on"
    "&lev_850_mb=on&lev_700_mb=on&lev_500_mb=on&lev_250_mb=on&lev_cloud_ceiling=on",
    "&var_TCDC=on&var_LCDC=on&var_MCDC=on&var_HCDC=on&var_VIS=on"
    "&lev_entire_atmosphere=on&lev_low_cloud_layer=on&lev_middle_cloud_layer=on"
    "&lev_high_cloud_layer=on&lev_surface=on",
};
#define FB_WX_NGROUPS ((int)(sizeof g_groups / sizeof g_groups[0]))

typedef struct {
    uint8_t *buf;
    size_t   n;
    int64_t  run_epoch, valid_epoch;
    long     expires;   /* wall clock after which the fast path re-probes NOMADS */
    int      refs;      /* live senders, plus one while this blob is g_cur */
    int      stale;     /* published without a successful live probe */
} fb_wx_blob;

/* Write-once before the pool starts. */
static char g_dir[320] = "/var/cache/fbtiles/wx";

/* g_cur, g_building and g_next_probe are guarded by g_mx; g_cv wakes the waiters of the one
 * in-flight build (one build, N waiters -- /wx has exactly one artefact, so a single in-flight slot
 * IS the per-key dedup). g_next_probe is the damper that keeps a failed probe from turning N
 * queued requests into N probe storms. */
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv = PTHREAD_COND_INITIALIZER;
static fb_wx_blob     *g_cur = 0;
static int             g_building = 0;
static long            g_next_probe = 0;

static long g_served = 0, g_built = 0, g_disk = 0, g_fetchfail = 0, g_decfail = 0,
            g_stale = 0, g_fallback = 0;
static void bump(long *c) { __atomic_fetch_add(c, 1, __ATOMIC_RELAXED); }

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_f32(uint8_t *p, float f) { uint32_t u; memcpy(&u, &f, sizeof u); put_u32(p, u); }
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static size_t plane_bytes(const fb_wx_field *d) {
    return (size_t)FB_WX_NX * FB_WX_NY * (d->bits / 8);
}
static size_t payload_start(void) {
    return (size_t)FB_WX_HEADER_BYTES + (size_t)FB_WX_NFIELDS * FB_WX_DESC_BYTES;
}
static size_t blob_bytes(void) {
    size_t n = payload_start();
    for (int i = 0; i < FB_WX_NFIELDS; i++) n += plane_bytes(&g_fields[i]);
    return n;
}
static uint32_t max_raw(const fb_wx_field *d) {
    uint32_t full = (d->bits == 8) ? 255u : 65535u;
    return d->missing_at > 0.0f ? full - 1u : full;
}
static float field_scale(const fb_wx_field *d) {
    return (float)(((double)d->vmax - d->vmin) / max_raw(d));
}

/* ---- building ---------------------------------------------------------------------------------- */

typedef struct {
    uint8_t *blob;
    int      got[FB_WX_NFIELDS];
    int64_t  run_epoch, valid_epoch;
    int      have_time;
} fb_wx_build;

static int match_field(const fb_grib2_field *f) {
    if (f->discipline != 0) return -1;
    for (int i = 0; i < FB_WX_NFIELDS; i++) {
        const fb_wx_field *d = &g_fields[i];
        if (f->category == d->cat && f->number == d->num && f->level_type == d->ltype &&
            fabs(f->level_value - d->grib_level) < 0.5)
            return i;
    }
    return -1;
}

static int wx_sink(const fb_grib2_field *f, const float *v, void *ud) {
    fb_wx_build *b = (fb_wx_build *)ud;
    int idx = match_field(f);
    if (idx < 0) return 0;   /* a record we did not ask for: skip, do not fail */

    if (f->nx != FB_WX_SRC_NX || f->ny != FB_WX_SRC_NY) return -1;   /* not the product we asked for */
    if (!(f->dlon > 0.0) || !(f->dlat < 0.0)) return -1;
    if (!b->have_time) {
        b->run_epoch = f->ref_epoch; b->valid_epoch = f->valid_epoch; b->have_time = 1;
    } else if (f->ref_epoch != b->run_epoch || f->valid_epoch != b->valid_epoch) {
        return -1;   /* records from two different runs must never be packed into one blob */
    }

    const fb_wx_field *d = &g_fields[idx];
    size_t off = payload_start();
    for (int i = 0; i < idx; i++) off += plane_bytes(&g_fields[i]);
    uint8_t *dst = b->blob + off;

    double  inv  = 1.0 / (double)field_scale(d);
    double  base = d->vmin;
    uint32_t hi  = max_raw(d);
    int      miss = d->missing_at > 0.0f;

    for (uint32_t j = 0; j < (uint32_t)FB_WX_NY; j++) {
        const float *row = v + (size_t)(j * FB_WX_STEP) * FB_WX_SRC_NX;
        for (uint32_t i = 0; i < (uint32_t)FB_WX_NX; i++) {
            float  s = row[i * FB_WX_STEP];
            uint32_t raw;
            if (miss && !(s < d->missing_at)) {   /* NaN lands here too, not on a bogus 0 m base */
                raw = hi + 1;
            } else {
                double q = ((double)s - base) * inv;
                /* NaN falls through both comparisons into the low clamp. */
                raw = !(q > 0.0) ? 0u : (q >= (double)hi ? hi : (uint32_t)(q + 0.5));
            }
            if (d->bits == 8) dst[(size_t)j * FB_WX_NX + i] = (uint8_t)raw;
            else              put_u16(dst + ((size_t)j * FB_WX_NX + i) * 2, (uint16_t)raw);
        }
    }
    b->got[idx] = 1;
    return 0;
}

static void write_header(uint8_t *blob, size_t n, int64_t run, int64_t valid) {
    memset(blob, 0, payload_start());
    put_u32(blob + 0, FB_WX_MAGIC);
    put_u16(blob + 4, FB_WX_FMT_VER);
    put_u16(blob + 6, FB_WX_HEADER_BYTES);
    put_u16(blob + 8, FB_WX_NX);
    put_u16(blob + 10, FB_WX_NY);
    put_f32(blob + 12, 90.0f);
    put_f32(blob + 16, 0.0f);
    put_f32(blob + 20, (float)(-0.25 * FB_WX_STEP));
    put_f32(blob + 24, (float)(0.25 * FB_WX_STEP));
    put_u32(blob + 28, (uint32_t)run);
    put_u32(blob + 32, (uint32_t)valid);
    put_u16(blob + 40, (uint16_t)FB_WX_NFIELDS);
    put_u16(blob + 42, FB_WX_DESC_BYTES);
    put_u32(blob + 44, (uint32_t)(n - payload_start()));
    blob[48] = FB_WX_HDR_FLAG_LON_WRAP;
    blob[49] = FB_WX_SOURCE_GFS_0P25;
    put_u16(blob + 50, FB_WX_STEP);

    size_t off = payload_start();
    for (int i = 0; i < FB_WX_NFIELDS; i++) {
        const fb_wx_field *d = &g_fields[i];
        uint8_t *p = blob + FB_WX_HEADER_BYTES + (size_t)i * FB_WX_DESC_BYTES;
        p[0] = d->var; p[1] = d->lev_kind;
        put_u16(p + 2, d->lev_value);
        p[4] = d->bits;
        p[5] = d->missing_at > 0.0f ? FB_WX_FLD_FLAG_MISSING : 0;
        put_u16(p + 6, (uint16_t)(d->missing_at > 0.0f ? max_raw(d) + 1 : 0));
        put_f32(p + 8, field_scale(d));
        put_f32(p + 12, d->vmin);
        put_u32(p + 16, (uint32_t)off);
        put_u32(p + 20, (uint32_t)plane_bytes(d));
        off += plane_bytes(d);
    }
}

typedef enum { FB_WX_OK = 0, FB_WX_NOT_READY, FB_WX_UNREACHABLE } fb_wx_fetch_rc;

static fb_wx_fetch_rc build_run(int64_t cycle, uint8_t **out, size_t *out_n,
                               int64_t *run_epoch, int64_t *valid_epoch) {
    struct tm tm;
    time_t t = (time_t)cycle;
    if (!gmtime_r(&t, &tm)) return FB_WX_NOT_READY;

    /* Zeroed, not just malloc'd: every plane is overwritten in full and a build with a missing
     * field is rejected outright, so this is belt and braces -- but it makes "no byte of a shipped
     * blob was ever uninitialised" a property of the allocation rather than of an argument. */
    size_t n = blob_bytes();
    uint8_t * blob = (uint8_t *)calloc(1, n);
    if (!blob) return FB_WX_UNREACHABLE;
    fb_wx_build b = {};
    b.blob = blob;

    fb_wx_fetch_rc rc = FB_WX_OK;
    for (int g = 0; g < FB_WX_NGROUPS && rc == FB_WX_OK; g++) {
        char url[1024];
        snprintf(url, sizeof url,
                 "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl"
                 "?dir=%%2Fgfs.%04d%02d%02d%%2F%02d%%2Fatmos&file=gfs.t%02dz.pgrb2.0p25.f000%s",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_hour, g_groups[g]);

        uint8_t *body = 0; size_t bn = 0;
        long code = fb_http_get_ex(url, &body, &bn, FB_WX_HTTP_TIMEOUT_S, FB_WX_MAX_GRIB);
        if (code == 0) { free(body); rc = FB_WX_UNREACHABLE; break; }
        /* NOMADS answers a not-yet-published cycle with 200 and an HTML/text explanation, so the
         * status code alone proves nothing. The GRIB magic is the discriminator -- this is the one
         * check that keeps an error page from ever becoming cached weather. */
        if (code != 200 || bn < 16 || memcmp(body, "GRIB", 4) != 0) {
            free(body);
            rc = (code >= 500) ? FB_WX_UNREACHABLE : FB_WX_NOT_READY;
            break;
        }
        int fields = fb_grib2_walk(body, bn, wx_sink, &b);
        free(body);
        if (fields < 0) { bump(&g_decfail); rc = FB_WX_NOT_READY; break; }
    }

    if (rc == FB_WX_OK) {
        for (int i = 0; i < FB_WX_NFIELDS; i++) if (!b.got[i]) { rc = FB_WX_NOT_READY; break; }
    }
    /* A cycle whose analysis time disagrees with the directory we asked for is not the run we
     * think it is; refuse rather than mislabel the header. */
    if (rc == FB_WX_OK && (!b.have_time || b.run_epoch != cycle)) rc = FB_WX_NOT_READY;
    if (rc != FB_WX_OK) { free(blob); return rc; }

    write_header(blob, n, b.run_epoch, b.valid_epoch);
    *out = blob; *out_n = n; *run_epoch = b.run_epoch; *valid_epoch = b.valid_epoch;
    return FB_WX_OK;
}

/* ---- disk ---------------------------------------------------------------------------------- */

static void cycle_path(int64_t cycle, char *p, size_t n) {
    struct tm tm; time_t t = (time_t)cycle;
    if (!gmtime_r(&t, &tm)) { snprintf(p, n, "%s/invalid", g_dir); return; }
    snprintf(p, n, "%s/gfs%d_%04d%02d%02d%02d_v%d.wxb", g_dir, FB_WX_STEP,
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, FB_WX_FMT_VER);
}

static uint8_t *disk_load(int64_t cycle, size_t *n, int64_t *run, int64_t *valid) {
    char path[400];
    cycle_path(cycle, path, sizeof path);
    struct stat st;
    size_t want = blob_bytes();
    if (stat(path, &st) != 0 || (size_t)st.st_size != want) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t * b = (uint8_t *)malloc(want);
    if (!b || fread(b, 1, want, f) != want) { free(b); fclose(f); return 0; }
    fclose(f);
    /* Trust nothing that came off disk either: a half-written or older-format file must look
     * absent, not decode into a wrong sky. */
    if (get_u32(b) != FB_WX_MAGIC || get_u16(b + 4) != FB_WX_FMT_VER ||
        get_u16(b + 8) != FB_WX_NX || get_u16(b + 10) != FB_WX_NY ||
        get_u16(b + 40) != FB_WX_NFIELDS || get_u32(b + 28) != (uint32_t)cycle) {
        free(b); return 0;
    }
    *n = want; *run = (int64_t)get_u32(b + 28); *valid = (int64_t)get_u32(b + 32);
    return b;
}

/* A GFS cycle is dead weight the moment the search window has walked past it, and unlike a map
 * tile it will never be asked for again. Pruning by NAME (no readdir, no stat loop): after a build
 * we unlink the cycles just outside the window, far enough back to also clean up after a few days
 * of downtime. Steady state is FB_WX_DISK_TRIES files, ~75 MB. */
#define FB_WX_PRUNE_SPAN 16

static void disk_prune(int64_t cycle) {
    for (int i = FB_WX_DISK_TRIES; i < FB_WX_DISK_TRIES + FB_WX_PRUNE_SPAN; i++) {
        char p[400];
        cycle_path(cycle - (int64_t)i * FB_WX_RUN_PERIOD_S, p, sizeof p);
        remove(p);
    }
}

static void disk_store(int64_t cycle, const uint8_t *b, size_t n) {
    char path[400], tmp[460];
    cycle_path(cycle, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    if (fwrite(b, 1, n, f) != n) { fclose(f); remove(tmp); return; }
    fclose(f);
    if (rename(tmp, path) != 0) { remove(tmp); return; }
    disk_prune(cycle);
}

/* ---- publication ------------------------------------------------------------------------------ */

/* Both need g_mx. */
static void blob_release_locked(fb_wx_blob *b) {
    if (!b) return;
    if (--b->refs == 0) { free(b->buf); free(b); }
}
static void publish_locked(fb_wx_blob *b) {
    fb_wx_blob *old = g_cur;
    g_cur = b;
    blob_release_locked(old);
}

static long expiry_for(int64_t run_epoch, long now) {
    long next = (long)(run_epoch + FB_WX_RUN_PERIOD_S + FB_WX_RUN_LATENCY_S);
    long floor_ = now + FB_WX_RETRY_S;   /* never re-probe more often than this while a run is late */
    return next > floor_ ? next : floor_;
}

static fb_wx_blob *make_blob(uint8_t *buf, size_t n, int64_t run, int64_t valid, int stale, long now) {
    fb_wx_blob * b = (fb_wx_blob *)calloc(1, sizeof *b);
    if (!b) { free(buf); return 0; }
    b->buf = buf; b->n = n; b->run_epoch = run; b->valid_epoch = valid;
    b->stale = stale; b->expires = expiry_for(run, now);
    b->refs = 1;   /* the reference held by g_cur */
    return b;
}

/* Returns a blob with one reference held for the caller, or 0 when no run exists anywhere. */
static fb_wx_blob *wx_acquire(void) {
    long now = (long)time(0);
    pthread_mutex_lock(&g_mx);
    for (;;) {
        if (g_cur && now < g_cur->expires) {
            fb_wx_blob *b = g_cur; b->refs++;
            pthread_mutex_unlock(&g_mx);
            return b;
        }
        if (g_building) { pthread_cond_wait(&g_cv, &g_mx); now = (long)time(0); continue; }
        if (now < g_next_probe) {   /* a probe just came up empty; don't repeat it per request */
            fb_wx_blob *b = g_cur;
            if (b) b->refs++;
            pthread_mutex_unlock(&g_mx);
            return b;
        }
        break;
    }
    g_building = 1;
    g_next_probe = now + FB_WX_RETRY_S;
    pthread_mutex_unlock(&g_mx);

    /* Newest cycle first, then backwards. Disk before network at every step, so a cycle we have
     * already built is never fetched twice; `live_ok` drops the moment NOMADS itself is
     * unreachable, which turns the rest of the walk into a disk-only search for something to fly
     * with instead of a retry storm. */
    int64_t newest = now - (now % FB_WX_RUN_PERIOD_S);
    int live_ok = 1, fell_back = 0;
    fb_wx_blob *made = 0;

    for (int i = 0; i < FB_WX_DISK_TRIES && !made; i++) {
        int64_t cyc = newest - (int64_t)i * FB_WX_RUN_PERIOD_S;

        size_t dn; int64_t drun, dvalid;
        uint8_t *db = disk_load(cyc, &dn, &drun, &dvalid);
        if (db) {
            bump(&g_disk);
            made = make_blob(db, dn, drun, dvalid, !live_ok, now);
            break;
        }
        if (!live_ok || i >= FB_WX_LIVE_TRIES) continue;

        uint8_t *nb = 0; size_t nn = 0; int64_t nrun = 0, nvalid = 0;
        fb_wx_fetch_rc rc = build_run(cyc, &nb, &nn, &nrun, &nvalid);
        if (rc == FB_WX_OK) {
            disk_store(cyc, nb, nn);
            bump(&g_built);
            made = make_blob(nb, nn, nrun, nvalid, 0, now);
            break;
        }
        if (rc == FB_WX_UNREACHABLE) { bump(&g_fetchfail); live_ok = 0; }
        else                         { fell_back = 1; }   /* cycle not published yet: normal */
    }
    if (fell_back && made) bump(&g_fallback);

    pthread_mutex_lock(&g_mx);
    if (made) {
        if (made->stale) bump(&g_stale);
        made->refs++;              /* the caller's reference, on top of g_cur's */
        publish_locked(made);
    } else if (g_cur) {
        /* Nothing anywhere: keep flying on what we have rather than blanking the sky. */
        g_cur->expires = now + FB_WX_RETRY_S;
        g_cur->stale = 1;
        made = g_cur; made->refs++;
        bump(&g_stale);
    }
    g_building = 0;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mx);
    return made;
}

static void wx_release(fb_wx_blob *b) {
    pthread_mutex_lock(&g_mx);
    blob_release_locked(b);
    pthread_mutex_unlock(&g_mx);
}

/* ---- endpoint ---------------------------------------------------------------------------------- */

static void iso8601(int64_t epoch, char *out, size_t n) {
    struct tm tm; time_t t = (time_t)epoch;
    if (!gmtime_r(&t, &tm)) { snprintf(out, n, "unknown"); return; }
    int y = tm.tm_year + 1900;
    if (y < 0) y = 0; else if (y > 9999) y = 9999;
    snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02dZ", y, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int fb_wx_init(const char *cache_dir) {
    if (cache_dir && *cache_dir) snprintf(g_dir, sizeof g_dir, "%s/wx", cache_dir);
    mkdir(g_dir, 0755);
    return 0;
}

void fb_wx_handle(int fd) {
    fb_wx_blob *b = wx_acquire();
    if (!b) {
        /* No GFS run reachable and none on disk. 503, never a cached body: the response has no
         * cacheable status, so nginx stores nothing. */
        fb_reply(fd, "503 Service Unavailable", "text/plain", "no gfs run available\n");
        return;
    }
    long now = (long)time(0);
    long ttl = b->expires - now;
    if (ttl < FB_WX_MIN_TTL_S) ttl = FB_WX_MIN_TTL_S;

    char run[80], valid[80], hdr[512];
    iso8601(b->run_epoch, run, sizeof run);
    iso8601(b->valid_epoch, valid, sizeof valid);
    int h = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nCache-Control: public, max-age=%ld\r\n"
        "X-Wx-Format: FBWX/%d\r\nX-Wx-Run: %s\r\nX-Wx-Valid: %s\r\nX-Wx-Stale: %d\r\n"
        "Connection: close\r\n\r\n", b->n, ttl, FB_WX_FMT_VER, run, valid, b->stale);
    if (fb_send_all(fd, hdr, (size_t)h) == 0 && fb_send_all(fd, b->buf, b->n) == 0)
        bump(&g_served);
    wx_release(b);
}

void fb_wx_stats(long *served, long *built, long *disk_hits, long *fetch_fail,
                 long *decode_fail, long *stale, long *run_fallback) {
    if (served)      *served      = __atomic_load_n(&g_served, __ATOMIC_RELAXED);
    if (built)       *built       = __atomic_load_n(&g_built, __ATOMIC_RELAXED);
    if (disk_hits)   *disk_hits   = __atomic_load_n(&g_disk, __ATOMIC_RELAXED);
    if (fetch_fail)  *fetch_fail  = __atomic_load_n(&g_fetchfail, __ATOMIC_RELAXED);
    if (decode_fail) *decode_fail = __atomic_load_n(&g_decfail, __ATOMIC_RELAXED);
    if (stale)       *stale       = __atomic_load_n(&g_stale, __ATOMIC_RELAXED);
    if (run_fallback) *run_fallback = __atomic_load_n(&g_fallback, __ATOMIC_RELAXED);
}
