/* FlightBox tiles — "does this imagery tile exist?", answered by Esri itself.
 * The pure parse is tilemap.h; the fetch/cache is tilemap.c. WHY is in tilemap.h.
 */
#ifndef FB_TILEMAP_API_H
#define FB_TILEMAP_API_H

/* 1 = exists, 0 = does not, -1 = we do not know (oracle unreachable, or its reply did not cover
 * this tile). -1 must send the caller to fetch anyway: a failing oracle may cost a wasted request,
 * never a wrongly skipped tile. Absence has to be POSITIVELY established or it is the overloaded
 * 404 again -- "no answer" quietly becoming "no tile". */
int  fb_tm_has(int z, long x, long y);
void fb_tm_stats(long *queries, long *hits, long *misses, long *learned, long *dropped);

#endif
