#define _GNU_SOURCE
#include "cache.h"
#include "tilesrc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>


static char g_dir[256] = "/var/cache/fbtiles";
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
    /* Once, before any thread exists: curl_global_init is explicitly NOT thread-safe, and doing it
     * lazily inside the first fetch is the classic way to get a rare, unreproducible crash. */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

static void cache_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.%s", g_dir, fb_src_kind_name(k), z, x, y, fb_src_ext(k));
}

/* The negative cache: an empty marker file beside where the tile would be. Its EXISTENCE is the
 * fact -- there is nothing to read, so there is nothing to get wrong.
 *
 * A zero-byte tile file would have been the obvious encoding and it is exactly wrong: st_size > 0
 * is how `fb_cache_ondisk` tells a tile from a truncated write, so an empty file would read as
 * "nothing here yet" -- ABSENT collapsing back into UNKNOWN, in the one place built to keep them
 * apart.
 *
 * This is PERMANENT, and that is a real bet: upstream could add a tile later (VersaTiles re-cuts,
 * Esri extends coverage) and we would never look again. Taken deliberately -- the alternative is
 * retrying every ocean tile forever, which is today's behaviour -- but it is why the marker is a
 * separate file and not a flag inside a tile: deleting the .absent files re-asks the whole world,
 * and that is the escape hatch. Nothing expires it automatically; nothing should, silently. */
static void absent_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.absent", g_dir, fb_src_kind_name(k), z, x, y);
}

static int is_absent(fb_tile_kind k, int z, long x, long y) {
    char p[400]; absent_path(k, z, x, y, p, sizeof p);
    struct stat st;
    return stat(p, &st) == 0;
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
 * This was `system("curl -s -f --compressed --max-time 20 -o tmp url")`. Per tile that meant:
 * fork+exec /bin/sh, sh parses the line, fork+exec curl, DNS, TCP, a full TLS handshake,
 * download, exit — and then the bytes travelled network -> disk -> memory, because the caller
 * read the file straight back. **No connection was ever reused.**
 *
 * Measured ON THIS SERVER, /t/ route (fetches, never bakes), 4x64 cold tiles per config, each run
 * on its OWN untouched region so nobody inherits a warm CDN edge, order ABBA-balanced against
 * drift:
 *     system(curl) : mean 8.13 s / 64 tiles  (sd 2.55)  = 127 ms/tile
 *     libcurl, kept: mean 3.47 s / 64 tiles  (sd 0.00)  =  54 ms/tile   -> 2.34x, ~73 ms/tile
 *
 * The SPREAD is the more telling half: reuse removes the handshake, and the handshake was where
 * the variance lived. And the per-tile cost FALLS with batch size (75/55/42 ms at 4/16/64 tiles) --
 * one connection amortised over more tiles, which is the mechanism showing itself.
 *
 * An earlier note here claimed 3.2x and "~116 s of the 433 s cold Hameln run". Both are withdrawn.
 * The 3.2x came from a throwaway /tmp benchmark, not from this server; the 116 s was that ratio
 * multiplied by a fetch count nobody had measured. What a cold Hameln run actually saves is still
 * unmeasured -- this number is about the fetch path alone.
 *
 * libcurl was already IN the container (libcurl.so.4) — curl was being started as a process
 * rather than linked. So this is a link, not a new dependency.
 *
 * The handle is thread-local and kept: that IS the fix. libcurl holds its connection pool per
 * easy handle, so the TLS session survives from one tile to the next only as long as the handle
 * does. A handle created per fetch would link libcurl and still pay every handshake — the same
 * bug with a nicer API.
 */
static pthread_key_t  g_key;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;
static void handle_free(void *h) { if (h) curl_easy_cleanup((CURL *)h); }
static void key_make(void)       { pthread_key_create(&g_key, handle_free); }

static CURL *handle(void) {
    pthread_once(&g_key_once, key_make);
    CURL *h = (CURL *)pthread_getspecific(g_key);
    if (h) return h;
    h = curl_easy_init();
    if (!h) return 0;
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, FB_FETCH_TIMEOUT_S);
    /* was --compressed: VersaTiles gzips as transfer encoding, and the cache must hold exactly
     * what a decoder expects, not the wire form. "" = every encoding libcurl was built with. */
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(h, CURLOPT_USERAGENT, "flightbox-tiles/1");
    pthread_setspecific(g_key, h);
    return h;
}

typedef struct { uint8_t *b; size_t n, cap; } buf;
static size_t sink(void *p, size_t sz, size_t nm, void *ud) {
    buf *o = (buf *)ud; size_t add = sz * nm;
    if (o->n + add > o->cap) {
        size_t cap = o->cap ? o->cap : 1 << 16;
        while (cap < o->n + add) cap <<= 1;
        uint8_t *t = realloc(o->b, cap);
        if (!t) return 0;                       /* short write -> libcurl aborts the transfer */
        o->b = t; o->cap = cap;
    }
    memcpy(o->b + o->n, p, add); o->n += add;
    return add;
}

/* BLOCKS on a miss (up to FB_FETCH_TIMEOUT_S). Only the prefetch worker may call this -- never
 * the accept() loop, which serves every client including the live flight view. */
int fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return 1;
    /* Known hole: do not ask upstream again. This is the only line that makes the negative cache
     * worth anything -- without it we would record the 404 and keep paying for it. */
    if (is_absent(k, z, x, y)) return 0;
    char url[600];
    if (!fb_src_url(k, z, x, y, url, sizeof url)) { g_fail++; return 0; }

    CURL *h = handle();
    if (!h) { g_fail++; return 0; }
    buf o = {0, 0, 0};
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sink);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &o);
    CURLcode rc = curl_easy_perform(h);
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);

    if (rc != CURLE_OK || code != 200 || o.n == 0) {
        free(o.b);
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
    if (!f || fwrite(o.b, 1, o.n, f) != o.n) { if (f) fclose(f); remove(tmp); free(o.b); g_fail++; return 0; }
    fclose(f);
    if (rename(tmp, path) != 0) { remove(tmp); free(o.b); g_fail++; return 0; }

    /* The bytes are already here -- the old code wrote them to disk and then read the same file
     * straight back. network -> memory -> disk, not network -> disk -> memory. */
    *out = o.b; *n = o.n;
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
