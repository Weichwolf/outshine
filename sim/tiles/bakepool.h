#ifndef FB_BAKEPOOL_H
#define FB_BAKEPOOL_H
#include "raster.h"

/* /bake used to be instant-202 on any cache miss (queue a background bake, poll again later) --
 * a relic from when a master bake cost seconds and the rasteriser wasn't thread-safe. Both are
 * gone (v7 native+supersampled bakes run 0.07-0.3s; raster.c is thread-safe since the data-race
 * fix). fb-tiles is now a real multi-threaded server: every connection (including /bake) runs on
 * a general worker from main.c's pool. /bake BLOCKS the calling worker until the bake is done --
 * no deadline, no 202; the boundary is the ordinary client/HTTP timeout. This module supplies the
 * one thing a plain blocking call doesn't give you for free: per-tile dedup, so N concurrent
 * requests for the SAME (kind,z,x,y,TS) run the actual raster+encode work exactly once and all N
 * get the result. */
void fb_bakepool_handle(int fd, fb_albedo_kind k, int z, long x, long y, int TS);

void fb_bakepool_stats(long *served);

#endif
