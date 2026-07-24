#ifndef FB_CACHE_H
#define FB_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include "tilesrc.h"

int  fb_cache_init(const char *dir);

int  fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

int  fb_cache_ondisk(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

typedef enum { FB_TILE_UNKNOWN = 0, FB_TILE_READY, FB_TILE_ABSENT } fb_tile_state;
fb_tile_state fb_cache_state(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

void fb_cache_stats(long *hits, long *fetches, long *fails);
long fb_cache_absent(void);
long fb_cache_absent_ttl(void);

#endif
