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

    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");

    curl_easy_setopt(h, CURLOPT_USERAGENT,
                     "flightbox-sim/1 (flight simulator; non-commercial research)");
    pthread_setspecific(fb_http__key, h);
    return h;
}

typedef struct { uint8_t *b; size_t n, cap; } fb_http_buf;

static size_t fb_http__sink(void *p, size_t sz, size_t nm, void *ud) {
    fb_http_buf *o = (fb_http_buf *)ud; size_t add = sz * nm;
    if (o->n + add + 1 > o->cap) {
        size_t cap = o->cap ? o->cap : 1 << 16;
        while (cap < o->n + add + 1) cap <<= 1;
        uint8_t *t = realloc(o->b, cap);
        if (!t) return 0;
        o->b = t; o->cap = cap;
    }
    memcpy(o->b + o->n, p, add); o->n += add;
    return add;
}

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

#endif
