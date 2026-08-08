#ifndef FB_PEAKS_H
#define FB_PEAKS_H
#include <stddef.h>

int  fb_peaks_init(const char *cache_dir);
int  fb_peaks_get(double lat, double lon, double radius_m, char **out, size_t *n);
void fb_peaks_stats(long *served, long *cells_cached, long *cells_fetched, long *fails);

#endif
