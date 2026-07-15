/* FlightBox tiles — bake a ground ALBEDO texture for one tile.
 *
 * This is the "prepares" half of fb-tiles' job description, which until now was not true: the
 * service handed out raw vector tiles and the browser rasterised them again on every single load.
 * The albedo is view-independent — it is what the ground *is*, not how it happens to be lit — so
 * it can be computed once and kept. Lighting stays where it belongs, per-pixel in the renderer's
 * shader from our own sun. Baking an albedo is not baking light.
 *
 * Two sources, one output:
 *   FB_ALBEDO_OSM    Shortbread vector -> cartography (landcover, water, buildings, roads)
 *   FB_ALBEDO_PHOTO  Esri aerial imagery -> the (TS/256)^2 children at z+log2(TS/256), blitted
 *
 * The caller gets a plain RGB buffer; encoding and disk caching are bake.c's business.
 */
#ifndef FB_RASTER_H
#define FB_RASTER_H
#include <stdint.h>

typedef enum { FB_ALBEDO_OSM = 0, FB_ALBEDO_PHOTO = 1 } fb_albedo_kind;

/* Render one tile's albedo into a TS x TS RGB buffer (3 bytes/px, caller-allocated).
 * Returns 1 on success, 0 if the source data could not be obtained at all.
 * A partial result is still a success: a missing photo child or an absent vector layer leaves the
 * base ground colour, which is a hole in the map, not a failure of the tile. */
int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, uint8_t *rgb);

#endif /* FB_RASTER_H */
