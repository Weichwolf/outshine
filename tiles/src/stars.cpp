#include "stars.h"
#include <stdio.h>
#include <stdlib.h>

/* The star catalogue is a bounded 53 KB natural constant, not a position-specific tile: baked at
 * build time by tools/build_stars.py and COPYed into the image, served from memory. No upstream, no
 * negative cache, no oracle -- a band is either present or empty, it never 404s. */
static struct { uint8_t *data; size_t n; } g_band[FB_STAR_BANDS];

void fb_stars_init(const char *dir)
{
    for (int b = 0; b < FB_STAR_BANDS; b++) {
        char path[512];
        snprintf(path, sizeof path, "%s/band%d.bin", dir, b);
        FILE *f = fopen(path, "rb");
        if (!f) { fprintf(stderr, "[fb-tiles] stars: no %s (band %d empty)\n", path, b); continue; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            g_band[b].data = (uint8_t *)malloc((size_t)sz);
            if (g_band[b].data && fread(g_band[b].data, 1, (size_t)sz, f) == (size_t)sz)
                g_band[b].n = (size_t)sz;
            else { free(g_band[b].data); g_band[b].data = NULL; }
        }
        fclose(f);
    }
}

int fb_stars_band(int band, const uint8_t **out, size_t *n)
{
    if (band < 0 || band >= FB_STAR_BANDS) return 0;
    *out = g_band[band].data;
    *n   = g_band[band].n;
    return 1;
}
