#ifndef FB_TILEMAP_H
#define FB_TILEMAP_H
#include <stddef.h>

typedef struct { long left, top; int width, height; } fb_tm_rect;

static int fb_tm_parse(const char *s, size_t n, fb_tm_rect *r, unsigned char *bits, int maxbits);

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

    if (s[n - 1] != 0) return 0;

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
    if (r->width > 4096 || r->height > 4096) return 0;

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
        if (*d != '0' && *d != '1') return 0;
        if (got >= want) return 0;
        bits[got++] = (unsigned char)(*d - '0');
        d++;

        if (*d >= '0' && *d <= '9') return 0;
    }

    if (got != want) return 0;
    return (int)got;
}

#endif
