#define _GNU_SOURCE
#include "cache.h"
#include "tilesrc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "http.h"
#include "tilemap_api.h"
#include <time.h>


#define FB_ABSENT_TTL_S (30L * 24 * 3600)
static char g_dir[256] = "/var/cache/fbtiles";
static long g_absent_ttl = FB_ABSENT_TTL_S;   /* TILES_ABSENT_TTL_S overrides; 0 = never cache a hole */
static long g_hits = 0, g_fetch = 0, g_fail = 0, g_absent = 0;

/* Upstream timeout. Was `--max-time 20` on a command line, i.e. a number in a string that no
 * compiler ever looked at. */
#define FB_FETCH_TIMEOUT_S 20L


int fb_cache_init(const char *dir) {
    if (dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    for (int i = 0; i < FB_TILE_KIND_COUNT; i++) {
        char sub[320]; snprintf(sub, sizeof sub, "%s/%s", g_dir, fb_src_kind_name((fb_tile_kind)i));
        mkdir(sub, 0755);
    }
    const char *t = getenv("TILES_ABSENT_TTL_S");
    if (t && *t) g_absent_ttl = atol(t);
    /* Once, before any thread exists: curl_global_init is explicitly NOT thread-safe, and doing it
     * lazily inside the first fetch is the classic way to get a rare, unreproducible crash. */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

static void cache_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.%s", g_dir, fb_src_kind_name(k), z, x, y, fb_src_ext(k));
}

/* The negative cache: an empty marker file beside where the tile would be. Its EXISTENCE is the
 * fact, its mtime is when we learned it.
 *
 * A zero-byte TILE file would be the obvious encoding and is exactly wrong: `st_size > 0` is how
 * fb_cache_ondisk tells a tile from a truncated write, so ABSENT would read as UNKNOWN inside the
 * one function built to keep them apart.
 *
 * It EXPIRES, because nobody in this business caches a negative forever and we should not be the
 * first: Squid's negative_ttl defaults to 0, and the whole mod_tile/renderd design is expiry
 * (render_expired, dirty timestamps). 30 days is derived, not picked: VersaTiles ships planet
 * builds roughly quarterly (osm.20260413, osm.20260105, osm.251006, osm.250728...), so a hole
 * CANNOT heal faster than that -- 30 days re-checks well inside one release cycle.
 *
 * The "~99% of holes are ocean, and ocean never heals" that used to justify the length here is
 * DEAD: measured, the ocean is not a hole at all (Shortbread carries it as an explicit polygon on
 * every zoom, so a missing tile can only be LAND). Which inverts the argument rather than removing
 * it -- unmapped land is exactly the thing that later gets mapped, so the expiry matters MORE than
 * the dead reason claimed, not less. The number survives its own justification; that is worth
 * noticing rather than tidying away. */

static void absent_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.absent", g_dir, fb_src_kind_name(k), z, x, y);
}

static int is_absent(fb_tile_kind k, int z, long x, long y) {
    char p[400]; absent_path(k, z, x, y, p, sizeof p);
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    if (time(0) - st.st_mtime < g_absent_ttl) return 1;
    remove(p);                     /* stale: ask upstream once more, and re-mark if still gone */
    return 0;
}

static void mark_absent(fb_tile_kind k, int z, long x, long y) {
    char p[400]; absent_path(k, z, x, y, p, sizeof p);
    FILE *f = fopen(p, "wb");     /* content-free on purpose: the name carries the whole fact */
    if (f) fclose(f);
}

static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

int fb_cache_ondisk(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    char path[400]; cache_path(k, z, x, y, path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0) return 0;
    *out = read_file(path, n);
    if (!*out) return 0;
    g_hits++;
    return 1;
}

/* ---- upstream fetch ------------------------------------------------------------------------
 *
 * The HTTP GET and its kept, thread-local libcurl handle live in http.h -- tilemap.c needs the
 * same one, and two copies would mean two connection pools with two lifetimes.
 *
 * This was `system("curl -s -f --compressed --max-time 20 -o tmp url")`: per tile a fork+exec
 * /bin/sh, a fork+exec curl, DNS, TCP, a full TLS handshake, download, exit -- and the bytes went
 * network -> disk -> memory, because the caller read the file straight back.
 *
 * Measured on this server, /t/ route, 4x64 cold tiles per config, each run on its OWN untouched
 * region (nobody inherits a warm CDN edge), order ABBA-balanced against drift:
 *     system(curl) : mean 8.13 s / 64 (sd 2.55) = 127 ms/tile
 *     libcurl kept : mean 3.47 s / 64 (sd 0.00) =  54 ms/tile   -> 2.34x
 * The SPREAD is the more telling half: reuse removes the handshake, and the handshake was where
 * the variance lived. Per-tile cost also falls with batch size (75/55/42 ms at 4/16/64) -- one
 * connection amortised, the mechanism showing itself.
 *
 * Two numbers once claimed here are withdrawn: "3.2x" (a /tmp benchmark, not this server) and
 * "116 s of the 433 s cold Hameln run" (that ratio times a fetch count nobody had measured).
 */
/* BLOCKS on a miss (up to FB_FETCH_TIMEOUT_S). Only the prefetch worker may call this -- never
 * the accept() loop, which serves every client including the live flight view. */
int fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return 1;
    /* Known hole: do not ask upstream again. This is the only line that makes the negative cache
     * worth anything -- without it we would record the 404 and keep paying for it. */
    if (is_absent(k, z, x, y)) return 0;

    /* Esri NEVER answers 404 -- measured, every case a 200, including z99/1/1. Above its coverage
     * it serves a 2521-byte placeholder card (sha256 9eafd300), which fb_cache_get would happily
     * store and serve forever as ground texture. So imagery absence cannot be learned from the
     * status code; it has to be ASKED, and Esri's tilemap is its own authoritative answer.
     * Only 0 acts. -1 (unknown) falls through and fetches: a failing oracle costs a request, not
     * correctness. */
    if (k == FB_TILE_IMAGERY && fb_tm_has(z, x, y) == 0) {
        g_absent++; mark_absent(k, z, x, y);
        return 0;
    }
    char url[600];
    if (!fb_src_url(k, z, x, y, url, sizeof url)) { g_fail++; return 0; }

    uint8_t *body = 0; size_t bn = 0;
    long code = fb_http_get(url, &body, &bn);
    if (code != 200 || bn == 0) {
        free(body);
        /* An upstream 404 is a DIFFERENT fact from "the network broke", and telling them apart is
         * what `204 No Content` and w3_avail{...,ABSENT} need a producer for -- today there is
         * none, so the renderer retries an ocean tile forever. Counted here rather than acted on:
         * a counter is honest, and turning it into a negative cache entry is its own step with
         * its own proof. -f used to swallow this distinction into one exit code. */
        if (code == 404) { g_absent++; mark_absent(k, z, x, y); } else g_fail++;
        return 0;
    }

    /* Write .tmp then rename: a killed fetch must never leave a truncated tile to be served
     * forever. The .tmp name carries the thread id because it MUST be unique -- with the single
     * worker it never collided, but two threads fetching the same tile would write one file and
     * rename a half of it into place. That is a race the thread pool would have introduced
     * silently, so it is closed before the pool exists rather than after. */
    char path[400], tmp[460];
    cache_path(k, z, x, y, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(body, 1, bn, f) != bn) { if (f) fclose(f); remove(tmp); free(body); g_fail++; return 0; }
    fclose(f);
    if (rename(tmp, path) != 0) { remove(tmp); free(body); g_fail++; return 0; }

    /* The bytes are already here -- the old code wrote them to disk and then read the same file
     * straight back. network -> memory -> disk, not network -> disk -> memory. */
    *out = body; *n = bn;
    g_fetch++;
    return 1;
}

fb_tile_state fb_cache_state(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return FB_TILE_UNKNOWN;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return FB_TILE_READY;
    /* Order matters: bytes win over a marker. If both somehow exist, the tile arrived after we gave
     * up on it, and the bytes are the newer truth. */
    if (is_absent(k, z, x, y)) return FB_TILE_ABSENT;
    return FB_TILE_UNKNOWN;
}

void fb_cache_stats(long *hits, long *fetches, long *fails) {
    if (hits) *hits = g_hits;
    if (fetches) *fetches = g_fetch;
    if (fails) *fails = g_fail;
}

/* Upstream said 404: the tile does not exist and never will. Separate from `fails` (network,
 * timeout, 5xx) because one is permanent and the other is worth retrying -- the whole distinction
 * this project has been missing. */
long fb_cache_absent(void) { return g_absent; }
long fb_cache_absent_ttl(void) { return g_absent_ttl; }
