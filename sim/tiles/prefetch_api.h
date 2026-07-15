/* FlightBox tiles — background prefetch, the bit main.c talks to.
 * The queue is pure (prefetch.h, unit-tested); the thread and policy are in prefetch.c. */
#ifndef FB_PREFETCH_API_H
#define FB_PREFETCH_API_H
#include "tilesrc.h"

void fb_pf_start(void);
/* We just served `served` for (z,x,y). If it was terrain, the renderer is about to ask for both
 * albedos of that ground, so bake them in the background NOW rather than inside the request that
 * asks for them -- a cold photo bake is 1.6 s, and this server serves nobody during it.
 * Never blocks the caller; drops work rather than queueing without bound. */
void fb_pf_warm(fb_tile_kind served, int z, long x, long y);
/* Both albedos for one tile at one texture size, plus its 8 neighbours. */
void fb_pf_warm_bakes(int z, long x, long y, int tex);
/* One raw tile, fetched on the worker. For routes that must answer without blocking. */
void fb_pf_fetch(fb_tile_kind k, int z, long x, long y);
void fb_pf_stats(long *queued, long *done, long *dropped, long *failed);

#endif
