#ifndef FB_ROUTE_H
#define FB_ROUTE_H
#include "cache.h"

int fb_route_tile(const char *path, fb_tile_kind *k, int *z, long *x, long *y);

int fb_query_double(const char *qs, const char *key, double *out);

#endif
