#ifndef FB_PREFETCH_API_H
#define FB_PREFETCH_API_H
#include "tilesrc.h"

void fb_pf_start(void);

void fb_pf_warm(fb_tile_kind served, int z, long x, long y);

void fb_pf_warm_bakes(int z, long x, long y, int tex);

void fb_pf_bake_one(int is_photo, int z, long x, long y, int tex);

void fb_pf_fetch(fb_tile_kind k, int z, long x, long y);
void fb_pf_stats(long *queued, long *done, long *dropped, long *failed);

void fb_pf_pool(int *threads, long *inflight_hits, long *absent);

#endif
