/* FlightBox tiles — generic tile fetch + disk cache.
 *
 * One place that knows where real-world tiles come from and how they are addressed. Both the
 * elevation service (which decodes DEM tiles) and the raw renderer routes go through here, so
 * upstream is touched once per tile, ever.
 *
 * The pure part — which kinds exist, their URLs and zoom limits — lives in tilesrc.h so it can
 * be unit-tested to 100%. This header is just the fetching and caching around it.
 */
#ifndef FB_CACHE_H
#define FB_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include "tilesrc.h"     /* what the sources are (pure); this header is only the I/O on top */

/* Set the cache directory and create the per-kind subdirectories. Returns 0 on success. */
int  fb_cache_init(const char *dir);
/* Fetch a tile (disk cache first, then upstream). On success returns 1 and hands over a
 * malloc'd buffer the caller must free. Returns 0 if the tile could not be obtained —
 * callers must treat that as "unknown", never as "empty". */
int  fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);
/* Disk only: 1 + malloc'd bytes if already cached, 0 otherwise. Never fetches, never blocks --
 * so it is the ONLY one of the two that may be called from the accept() loop. */
int  fb_cache_ondisk(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);
void fb_cache_stats(long *hits, long *fetches, long *fails);

/* What this server KNOWS about a tile, disk only. Three states because there are three facts, and
 * collapsing any two of them is the bug this project keeps rediscovering:
 *
 *   READY   we have the bytes.                                    -> 200
 *   ABSENT  upstream said 404. There is no such tile, ever.       -> 204
 *   UNKNOWN we have not looked, or looking has not finished yet.   -> 202 (ask again)
 *
 * ABSENT vs UNKNOWN is the whole point. They were ONE state (`!fb_cache_ondisk`) and the server
 * answered 404 for both -- the browser cached that as a permanent hole and a cold region never
 * loaded. The fix inverted it: everything unknown means "ask again", which made an ocean tile
 * retry FOREVER instead. Both failures are the same missing state, seen from the two sides.
 *
 * An enum rather than an int so `-Wswitch` (already in our -Wall) turns "one state too few" into a
 * build failure. That was refused once before, correctly: back then ABSENT had no producer -- the
 * wire could not carry it and `curl -s -f` had already thrown the 404 away, so the type would have
 * claimed more than reality could deliver. It has one now (cache.c reads CURLINFO_RESPONSE_CODE
 * and records the 404), which is why the type comes NOW and not then. */
typedef enum { FB_TILE_UNKNOWN = 0, FB_TILE_READY, FB_TILE_ABSENT } fb_tile_state;
fb_tile_state fb_cache_state(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n);

/* Upstream answered 404: this tile does not exist and never will. Deliberately NOT folded into
 * `fails` -- "not there yet" and "not there, ever" are different facts, and merging them into one
 * number is the same mistake the overloaded 404 made on our own wire. */
long fb_cache_absent(void);

#endif /* FB_CACHE_H */
