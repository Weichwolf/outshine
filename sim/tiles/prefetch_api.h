/* FlightBox tiles — background prefetch, the bit main.c talks to.
 * The queue is pure (prefetch.h, unit-tested); the thread and policy are in prefetch.c. */
#ifndef FB_PREFETCH_API_H
#define FB_PREFETCH_API_H
#include "tilesrc.h"

void fb_pf_start(void);
/* We just served `served` for (z,x,y): warm the other kinds for the same ground, in the
 * background. Never blocks the caller; drops work rather than queueing without bound. */
void fb_pf_warm(fb_tile_kind served, int z, long x, long y);
void fb_pf_stats(long *queued, long *done, long *dropped, long *failed);

#endif
