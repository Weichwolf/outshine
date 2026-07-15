/* FlightBox tiles — request parsing.
 *
 * Pure string handling, split out from the server loop so it can be unit-tested: a route parser
 * that quietly mis-reads a coordinate produces a plausible-looking wrong tile, which is exactly
 * the class of bug that hides for weeks.
 */
#ifndef FB_ROUTE_H
#define FB_ROUTE_H
#include "cache.h"

/* Parse "/t/<kind>/<z>/<x>/<y>" — a trailing ".ext" is accepted and ignored.
 * Rejects anything malformed, negative, out of the zoom range, or outside the 2^z grid.
 * Returns 1 on success. */
int fb_route_tile(const char *path, fb_tile_kind *k, int *z, long *x, long *y);

/* Read a floating-point query parameter out of "lat=1.5&lon=2" (no leading '?').
 * Returns 1 on success, 0 if absent or unparseable. Matches whole keys only, so "lat" does
 * not match "sublat". */
int fb_query_double(const char *qs, const char *key, double *out);

#endif /* FB_ROUTE_H */
