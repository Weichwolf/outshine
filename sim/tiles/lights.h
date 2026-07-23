#ifndef FB_LIGHTS_H
#define FB_LIGHTS_H
#include <stdint.h>
#include <stddef.h>

int fb_lights_init(const char *dir);

/* Fetch/build the binary night-light list for one tile (see lights.c for the wire format). Hands
 * over a malloc'd buffer the caller frees. Returns 1 with the output buffer/length set -- a
 * legitimately empty tile
 * (ocean, no lightable features) still returns 1 with a 4-byte count=0 body. Returns 0 only when the
 * underlying vector data itself is absent (no OSM tile behind this point at all): caller replies 204,
 * same absent-vs-empty distinction /t/vector already makes. */
int fb_lights_get(int z, long x, long y, uint8_t **out, size_t *n);

int fb_lights_ondisk(int z, long x, long y, uint8_t **out, size_t *n);

void fb_lights_stats(long *baked, long *served);

#endif
