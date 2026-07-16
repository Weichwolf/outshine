#ifndef FB_TILEMAP_API_H
#define FB_TILEMAP_API_H

int  fb_tm_has(int z, long x, long y);
void fb_tm_stats(long *queries, long *hits, long *misses, long *learned, long *dropped);

#endif
