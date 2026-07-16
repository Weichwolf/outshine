/* FlightBox tiles — the one HTTP GET.
 *
 * Extracted from cache.c when tilemap.c turned out to need the same thing. Copying it would have
 * meant two connection pools with two lifetimes, and the whole point of this code is that the pool
 * survives between requests: libcurl keeps its connections in the EASY HANDLE, so a handle per
 * fetch links the library and still pays every TLS handshake. Measured before this existed:
 * 127 ms/tile with a handle per fetch (or a curl process), 54 ms/tile with one kept -- 2.34x, and
 * the variance goes with it, because the variance WAS the handshake.
 *
 * The handle is thread-local: the prefetch pool has N workers, each gets its own pool of
 * connections, and nothing is shared between them. That is also why there is no mutex here.
 */
#ifndef FB_HTTP_H
#define FB_HTTP_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>

#define FB_HTTP_TIMEOUT_S 20L

static pthread_key_t  fb_http__key;
static pthread_once_t fb_http__once = PTHREAD_ONCE_INIT;
static void fb_http__free(void *h) { if (h) curl_easy_cleanup((CURL *)h); }
static void fb_http__mk(void)      { pthread_key_create(&fb_http__key, fb_http__free); }

static CURL *fb_http_handle(void) {
    pthread_once(&fb_http__once, fb_http__mk);
    CURL *h = (CURL *)pthread_getspecific(fb_http__key);
    if (h) return h;
    h = curl_easy_init();
    if (!h) return 0;
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, FB_HTTP_TIMEOUT_S);
    /* "" = every encoding libcurl was built with. VersaTiles gzips as a TRANSFER encoding, and the
     * cache must hold what a decoder expects, not the wire form. */
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    /* The prevailing norm (OSM's tile usage policy, which VersaTiles/Esri/AWS do not restate but
     * which is what tile consumers are judged by) requires a User-Agent that clearly identifies
     * the application -- explicitly NOT a library default. `flightbox-tiles/1` said nothing about
     * who we are or what we are for.
     *
     * No contact address: that is the user's to give, not ours to invent, and it would put a
     * private address in the logs of three third parties. The trade is deliberate and worth
     * knowing: without a contact, anyone we bother can only block us, never ask. */
    curl_easy_setopt(h, CURLOPT_USERAGENT,
                     "flightbox-sim/1 (flight simulator; non-commercial research)");
    pthread_setspecific(fb_http__key, h);
    return h;
}

typedef struct { uint8_t *b; size_t n, cap; } fb_http_buf;

static size_t fb_http__sink(void *p, size_t sz, size_t nm, void *ud) {
    fb_http_buf *o = (fb_http_buf *)ud; size_t add = sz * nm;
    if (o->n + add + 1 > o->cap) {                  /* +1: room for the NUL fb_http_get appends */
        size_t cap = o->cap ? o->cap : 1 << 16;
        while (cap < o->n + add + 1) cap <<= 1;
        uint8_t *t = realloc(o->b, cap);
        if (!t) return 0;                           /* short write -> libcurl aborts the transfer */
        o->b = t; o->cap = cap;
    }
    memcpy(o->b + o->n, p, add); o->n += add;
    return add;
}

/* GET into memory. Returns the HTTP status (0 = the request never completed), and hands over a
 * malloc'd buffer the caller must free. The buffer is NUL-terminated one past `*n`, so a JSON
 * reply can be handed straight to a string parser without a copy -- the bytes themselves are
 * untouched, so a JPEG is still exactly `*n` bytes.
 *
 * A 404 is a RESULT, not an error: it is returned as 404 with no body, because "upstream does not
 * have this" and "the fetch broke" are different facts. `curl -f` used to collapse them into one
 * exit code, which is how an ocean of holes became indistinguishable from a broken network. */
static long fb_http_get(const char *url, uint8_t **out, size_t *n) {
    *out = 0; *n = 0;
    CURL *h = fb_http_handle();
    if (!h) return 0;
    fb_http_buf o = {0, 0, 0};
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, fb_http__sink);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &o);
    CURLcode rc = curl_easy_perform(h);
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    if (rc != CURLE_OK) { free(o.b); return 0; }
    if (o.b) o.b[o.n] = 0;
    *out = o.b; *n = o.n;
    return code;
}

#endif /* FB_HTTP_H */
