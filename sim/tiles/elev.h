#ifndef FB_ELEV_H
#define FB_ELEV_H

int  fb_elev_init(const char *cache_dir);

int  fb_elev_at(double lat, double lon, double *out);

int  fb_elev_at_blocking(double lat, double lon, double *out, int deadline_ms);

void fb_elev_stats(long *hits, long *misses, long *fetch_fail);

#endif
