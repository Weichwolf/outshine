/* FlightBox tiles — tile fetch + disk cache. Sources and their URLs are in tilesrc.h (pure). */
#ifndef FB_CACHE_H
#define FB_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include "tilesrc.h"

int  fb_cache_init(const char *dir);

/* Blocks up to 20 s on a miss. Prefetch workers only — never the accept() loop. */
int  fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

/* Disk only, never blocks. */
int  fb_cache_ondisk(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

/* READY -> 200, ABSENT -> 204, UNKNOWN -> 202 (ask again). Disk only.
 * UNKNOWN must never collapse into ABSENT: absence is positively established or it is not known. */
typedef enum { FB_TILE_UNKNOWN = 0, FB_TILE_READY, FB_TILE_ABSENT } fb_tile_state;
fb_tile_state fb_cache_state(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

void fb_cache_stats(long *hits, long *fetches, long *fails);
long fb_cache_absent(void);       /* upstream 404s; separate from fails, which are retryable */
long fb_cache_absent_ttl(void);   /* seconds; the 204's max-age must not exceed it */

#endif /* FB_CACHE_H */
