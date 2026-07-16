/* FlightBox tiles — Esri's tilemap: which imagery tiles actually EXIST.
 *
 * WHY THIS EXISTS. Esri never answers 404. Measured, every case a 200:
 *     z12 ocean   200  1652 B  ddd50da0   a real (constant) ocean tile
 *     z12 Hameln  200 19790 B  db2845a7   a real image
 *     z21 Hameln  200  2521 B  9eafd300   a PLACEHOLDER
 *     z24, x out of range, z99/1/1        the same placeholder, 200 every time
 * So `fb_cache_get` sees 200 + bytes, writes them to disk and serves them forever as the photo
 * albedo. Above Esri's coverage the simulator would show Esri's "no data" card as ground, cached
 * permanently, with no counter saying so. A 404 would have been a gift; a 200 is a lie we store.
 *
 * `maxz = 19` in tilesrc.c stands in for this and cannot: coverage is LOCAL. z19 exists over
 * Hameln; over an empty quarter it may stop at z16, and one global constant cannot know that.
 *
 * The tilemap is Esri's own answer: 1024 tiles per request, 80 ms, ~2 KB. It is authoritative --
 * z21 over Hameln returns all 0, exactly where the placeholder is served.
 *
 * This header is the PURE half (parse a response), so the one thing that can silently corrupt the
 * oracle is testable without a network. The fetching and caching live in tilemap.c.
 */
#ifndef FB_TILEMAP_H
#define FB_TILEMAP_H
#include <stddef.h>

/* The rectangle a tilemap response actually describes. NOT the one you asked for.
 *
 * THE TRAP, measured: a 32x32 request at a coastline came back with `"adjusted": true` and
 * `location: {top, left, width: 32, height: 4}` -- 128 entries, not 1024. Esri shrinks a request
 * at its discretion and says so only here. Indexing `data[row*32 + col]` against your own request
 * reads garbage from the first adjusted answer onward, and only at some tiles, which is the
 * shape of a bug nobody finds. Always read the rectangle back out of the reply. */
typedef struct { long left, top; int width, height; } fb_tm_rect;

/* Parse `{"data":[0,1,...],"location":{"left":L,"top":T,"width":W,"height":H},"valid":true}`.
 *
 * Writes the described rectangle to *r and its w*h entries (0/1, row-major from top-left) into
 * `bits`. Returns the number of entries written, or 0 if the reply is unusable -- which includes
 * `valid: false`, a data array whose length disagrees with location (the reply contradicting
 * itself), or more entries than `maxbits`. Zero always means "learned nothing", never "no tiles":
 * the caller must fall back to asking, never to assuming absence. Getting that backwards is the
 * overloaded-404 bug wearing a different hat.
 *
 * Deliberately not a JSON parser: two fixed shapes, one producer, and a dependency would be a
 * bigger surface than the thing it parses. It is strict about what it does not recognise. */
static int fb_tm_parse(const char *s, size_t n, fb_tm_rect *r, unsigned char *bits, int maxbits);

/* ---- implementation (header-only so the unit test compiles it natively, no network) ---- */
#include <string.h>
#include <stdlib.h>

static long fb_tm__num(const char *s, const char *key, int *ok) {
    const char *p = strstr(s, key);
    if (!p) { *ok = 0; return 0; }
    p += strlen(key);
    while (*p == ' ' || *p == ':' || *p == '"') p++;
    char *end = 0;
    long v = strtol(p, &end, 10);
    *ok = (end != p);
    return v;
}

static int fb_tm_parse(const char *s, size_t n, fb_tm_rect *r, unsigned char *bits, int maxbits) {
    if (!s || !n || !r || !bits) return 0;
    /* The reply must be NUL-terminated for strstr; callers hand us a buffer they own. */
    if (s[n - 1] != 0) return 0;

    /* "valid": false means Esri could not answer. Not "there are no tiles". */
    const char *v = strstr(s, "\"valid\"");
    if (v && strstr(v, "false") && (size_t)(strstr(v, "false") - v) < 12) return 0;

    const char *loc = strstr(s, "\"location\"");
    if (!loc) return 0;
    int ok1, ok2, ok3, ok4;
    r->left   = fb_tm__num(loc, "\"left\"",   &ok1);
    r->top    = fb_tm__num(loc, "\"top\"",    &ok2);
    r->width  = (int)fb_tm__num(loc, "\"width\"",  &ok3);
    r->height = (int)fb_tm__num(loc, "\"height\"", &ok4);
    if (!ok1 || !ok2 || !ok3 || !ok4) return 0;
    if (r->width <= 0 || r->height <= 0) return 0;
    if (r->width > 4096 || r->height > 4096) return 0;          /* absurd: refuse rather than trust */

    long want = (long)r->width * r->height;
    if (want > maxbits) return 0;

    const char *d = strstr(s, "\"data\"");
    if (!d) return 0;
    d = strchr(d, '[');
    if (!d) return 0;
    d++;

    long got = 0;
    for (;;) {
        while (*d == ' ' || *d == ',' || *d == '\n' || *d == '\r' || *d == '\t') d++;
        if (*d == ']' || *d == 0) break;
        if (*d != '0' && *d != '1') return 0;                   /* not the alphabet we expect */
        if (got >= want) return 0;                              /* more data than location claims */
        bits[got++] = (unsigned char)(*d - '0');
        d++;
        /* A multi-digit token would mean the format is not what we think it is. */
        if (*d >= '0' && *d <= '9') return 0;
    }
    /* The reply must agree with itself. Fewer entries than the rectangle claims is not a shorter
     * rectangle -- it is a response we do not understand, and guessing would put the wrong tiles
     * in the cache at the wrong coordinates. */
    if (got != want) return 0;
    return (int)got;
}

#endif /* FB_TILEMAP_H */
