#ifndef FB_RASTER_H
#define FB_RASTER_H
#include <stdint.h>

typedef enum { FB_ALBEDO_OSM = 0, FB_ALBEDO_PHOTO = 1 } fb_albedo_kind;

/* TS = the actual raster buffer size this call fills (native resolution). lod_ts = the size the
 * result will ultimately be SERVED at (== TS for a direct native bake; TS/2 when bake.c supersamples
 * at 2x and box-downscales afterward). OSM feature-LOD (raster.c) decides what to skip based on
 * lod_ts -- a building or minor road invisible at the SERVED size is skipped regardless of how large
 * the native raster happens to be. PHOTO ignores lod_ts (no vector features to drop). */
int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, int lod_ts, uint8_t *rgb);

/* Linear-correct 2x-box downsample, srcTS -> dstTS (both powers of two, dstTS <= srcTS). */
int fb_raster_downscale(const uint8_t *src, int srcTS, uint8_t *dst, int dstTS);

#endif
