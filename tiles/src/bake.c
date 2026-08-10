#define _GNU_SOURCE
#include "bake.h"
#include "raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../vendor/stb_image_write.h"
#include "../vendor/stb_image.h"

static char g_dir[256] = "/var/cache/fbtiles";
static long g_hits = 0, g_bakes = 0, g_fail = 0;
static long g_native_bakes = 0, g_super_bakes = 0;

/* Bumped whenever raster.c's OSM coloring or bake strategy changes in a way that must invalidate old
 * disk bakes -- a suffix in the filename rather than a cache purge, so old bakes are simply orphaned
 * (never matched again, reclaimable independently) instead of needing a stop-the-world delete of a
 * warm, months-old cache. PHOTO bakes stay unversioned: the background pre-fill only shows through
 * the rare uncovered span in a photo mosaic, a minor and self-healing cosmetic detail, not worth
 * doubling a 2+ GB cache for. v6: 20 style.h gap entries. v7: this file's native-raster + Feature-LOD
 * rework (see fb_bake_get) replacing the always-2048-master v4/v5 approach -- a separate bump from v6
 * even though both landed in the same session, so v6 stays gate-approved on its own without the LOD
 * rework riding along under its version number. v8: analytical edge AA (draw.h) at TS>=2048 only,
 * bake_supersampled still default below that. v9: bake_supersampled retired as the default at EVERY
 * size (see fb_bake_get) -- a separate bump from v8 even though the actual PIXELS at TS>=2048 didn't
 * change, because the ones below 2048 did (native+AA replacing supersample+downscale there too), and
 * v8's on-disk files for those sizes must not be served as if they were v9's. v10 (font-quality
 * signed-area coverage rasteriser, FreeType/font-rs cell-accumulation technique, fixing v8/v9's
 * blind spot on shallow near-horizontal slopes) was measured, not shipped: two dedicated
 * optimisation rounds still left it 44-107% over the bake-time budget on every tex size, and the
 * user's final call was that raw bake speed matters more here than edge softness. v11: draw.h back
 * to a fully hard (non-anti-aliased) rasteriser -- v8/v9's AA is ALSO removed, not just v10's; only
 * the structural wins survive (native-resolution rendering here, Feature-LOD, the MT pool/blocking/
 * nginx cache elsewhere). Pixel content changes again, hence the bump (v10 itself never went live,
 * so there's no v10 on-disk content to worry about orphaning). PNG encode settings changed too (see
 * fb_bake_init) -- doesn't affect decodability, only bytes-on-the-wire, but stays under the same
 * version bump for one clean cutover rather than a second one right behind it.
 * v12: die Ebene "ocean" wird ueberhaupt erst gezeichnet. Sie war nie abgefragt, also kam JEDE reine
 * Seekachel als Vorfuell-Beige heraus — [MESS] z7/49/48 (Atlantik) und z7/65/39 (Nordsee) zu 100 %
 * (235,231,221), waehrend die Vektorquelle fuer den Atlantik genau eine Ebene liefert und sie "ocean"
 * heisst. Pixelinhalt aendert sich auf jeder Kachel mit Seeanteil, also Bump. The define lives in
 * style_ver.h so the CLIENT bakes it into the URL (?v=) — bake responses are immutable-cached, so a
 * style/bake-strategy bump must also change the URL, not just the disk filename. */
#include "style_ver.h"
#define FB_SUPERSAMPLE_BELOW_TS 2048   /* threshold bake_supersampled (below) used to apply to --
                                        * kept only as the gate for the TILES_FORCE_SUPERSAMPLE A/B
                                        * escape hatch, see fb_bake_get. */

/* v9: native-resolution rasterisation replaced the 2x-supersample-then-box-downscale path (bake_
 * supersampled) at every size, not just >=2048 (v8). Small tiles used to look softer than the top
 * size on purpose (box-downscale blurs interior texture, not just edges) -- pure loss regardless of
 * whether the edges themselves are AA'd (v9-v10) or hard (v11): it cost 4x the fill work (512^2
 * raster for a 256 request) for a blurrier result than rendering 256^2 natively. Measured: small
 * sizes are 28-42% FASTER this way too (less raster area, same LOD gate) -- not a tradeoff, so this
 * stays in v11 even though the AA it was originally paired with does not. bake_supersampled itself
 * stays in the file, unreachable by default: a documented, cheap A/B escape hatch
 * (TILES_FORCE_SUPERSAMPLE=1) for re-comparing against the old path, not a live code path. */
static int fb_force_supersample(void){
    const char *e = getenv("TILES_FORCE_SUPERSAMPLE");
    return e && *e && *e != '0';
}

/* Stage-profiled a bake (v11 max-speed trim): PNG encode, not the rasteriser, was the dominant
 * cost -- at tex=4096, fill+lines together took ~0.13s while stb_image_write's DEFAULT settings
 * took 0.7-2.1s to encode the SAME pixels (measured on real Bern-area tiles, several samples). Two
 * separate stb_image_write knobs, measured independently:
 *   - stbi_write_force_png_filter: defaults to -1, meaning "try all 5 PNG filter types per scanline
 *     and keep whichever compresses smallest" -- that heuristic itself, not the zlib deflate it
 *     feeds, was most of the cost. Forcing filter 0 (None, no per-pixel prediction at all) measured
 *     ~4.2x faster for map-tile content (large flat-color regions don't benefit much from Sub/Up/
 *     Average/Paeth prediction anyway) at only ~1-2% larger files.
 *   - stbi_write_png_compression_level: defaults to 8 (near-max zlib effort). Dropping to 1
 *     (fastest) on top of filter=0 measured a further ~20-25% encode speedup for ~4% more bytes.
 * Combined: ~5x faster encode for ~4.5% larger PNGs on the measured tiles -- comfortably inside "not
 * >2x bytes for <20% speed" and then some. Both are plain global ints in stb_image_write.h, set
 * ONCE here before any worker thread exists (matches fb_elev_init/fb_stars_init's
 * same single-threaded-startup pattern) -- read-only from every bake call afterward, so no lock
 * needed and no race: this is a fixed, measured constant, not a per-request or operator-tunable
 * knob (no TILES_* env var for it, unlike the pool sizes elsewhere in this server). */
int fb_bake_init(const char *dir){
    if(dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    char sub[320];
    snprintf(sub, sizeof sub, "%s/bake_osm",   g_dir); mkdir(sub, 0755);
    snprintf(sub, sizeof sub, "%s/bake_photo", g_dir); mkdir(sub, 0755);
    stbi_write_force_png_filter = 0;
    stbi_write_png_compression_level = 1;
    return 0;
}

static void bake_path(fb_albedo_kind k, int z, long x, long y, int TS, char *p, size_t n){
    if(k==FB_ALBEDO_PHOTO)
        snprintf(p, n, "%s/bake_photo/%d_%d_%ld_%ld.jpg", g_dir, TS, z, x, y);
    else
        snprintf(p, n, "%s/bake_osm/v%d_%d_%d_%ld_%ld.png", g_dir, FB_OSM_STYLE_VER, TS, z, x, y);
}

static uint8_t *read_file(const char *p, size_t *n){
    FILE *f = fopen(p, "rb"); if(!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if(sz <= 0){ fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if(!b || fread(b, 1, (size_t)sz, f) != (size_t)sz){ free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

typedef struct { uint8_t *b; size_t n, cap; } membuf;
static void mem_write(void *ctx, void *data, int size){
    membuf *m = (membuf*)ctx;
    if(m->n + (size_t)size > m->cap){
        size_t cap = (m->cap ? m->cap*2 : 1<<16);
        while(cap < m->n + (size_t)size) cap *= 2;
        uint8_t *t = realloc(m->b, cap); if(!t) return;
        m->b = t; m->cap = cap;
    }
    memcpy(m->b + m->n, data, (size_t)size); m->n += (size_t)size;
}

int fb_bake_ondisk(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    if(TS < 64 || TS > 4096 || (TS & (TS-1))) return 0;
    char path[400]; bake_path(k, z, x, y, TS, path, sizeof path);
    struct stat st;
    if(stat(path, &st) != 0 || st.st_size <= 0) return 0;
    *out = read_file(path, n);
    if(!*out) return 0;
    __atomic_fetch_add(&g_hits, 1, __ATOMIC_RELAXED);
    return 1;
}

/* Write `rgb` (TS*TS*3) to its cache path as PNG (OSM) or JPEG (PHOTO); hands back the encoded
 * bytes too so the caller doesn't have to re-read its own write. */
static int encode_and_store(fb_albedo_kind k, int z, long x, long y, int TS,
                             const uint8_t *rgb, uint8_t **out, size_t *n){
    membuf m = {0};
    if(k == FB_ALBEDO_PHOTO) stbi_write_jpg_to_func(mem_write, &m, TS, TS, 3, (void*)rgb, 88);
    else                     stbi_write_png_to_func(mem_write, &m, TS, TS, 3, (void*)rgb, TS*3);
    if(!m.n){ free(m.b); return 0; }

    char path[400]; bake_path(k, z, x, y, TS, path, sizeof path);
    /* pthread_self()-suffixed, like cache.c: two threads racing the same tile (the rare
     * duplicate-owner window under heavy concurrency, see bakepool.c) must not share one tmp path. */
    char tmp[460]; snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if(f){
        int ok = fwrite(m.b, 1, m.n, f) == m.n;
        fclose(f);
        if(ok) rename(tmp, path); else remove(tmp);
    }
    *out = m.b; *n = m.n;
    return 1;
}

/* Direct 1x native rasterisation: every PHOTO size (its mosaic already zooms to fit TS -- see
 * raster.c) and every OSM size (native rendering is the ONLY OSM path by default -- see fb_bake_
 * get). lod_ts==TS: served size IS the native size, so Feature-LOD sees the same size it's drawing
 * at (nothing to drop that wouldn't also be dropped at serve time -- correct at every size, since a
 * sub-1.5px² feature is genuinely negligible, not just "small for a downscale"). Smaller TS means
 * less raster area to fill, not a coarser blur: no box-downscale softens the interior. */
static int bake_native(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    uint8_t *rgb = malloc((size_t)TS*TS*3);
    if(!rgb){ __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }
    if(!fb_raster_bake(k, z, x, y, TS, TS, rgb)){
        free(rgb); __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0;
    }
    int ok = encode_and_store(k, z, x, y, TS, rgb, out, n);
    free(rgb);
    if(!ok) __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED);
    return ok;
}

/* Superseded by native rendering at every size (see fb_bake_get) -- kept only behind
 * TILES_FORCE_SUPERSAMPLE for A/B re-comparison against the old 2x-supersample-then-box-downscale
 * path. Rasterises FRESH at native=2*TS and box-halves down to TS, no shared/cached intermediate. */
static int bake_supersampled(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    int native = TS*2;
    uint8_t *raw = malloc((size_t)native*native*3);
    if(!raw){ __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }
    if(!fb_raster_bake(k, z, x, y, native, TS, raw)){
        free(raw); __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0;
    }

    uint8_t *rgb = malloc((size_t)TS*TS*3);
    int ok = rgb && fb_raster_downscale(raw, native, rgb, TS);
    free(raw);
    if(!ok){ free(rgb); __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }

    ok = encode_and_store(k, z, x, y, TS, rgb, out, n);
    free(rgb);
    if(!ok) __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED);
    return ok;
}

int fb_bake_get(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    if(TS < 64 || TS > 4096 || (TS & (TS-1))) return 0;
    if(fb_bake_ondisk(k, z, x, y, TS, out, n)) return 1;

    if(k == FB_ALBEDO_OSM && TS < FB_SUPERSAMPLE_BELOW_TS && fb_force_supersample()){
        if(!bake_supersampled(k, z, x, y, TS, out, n)) return 0;
        __atomic_fetch_add(&g_super_bakes, 1, __ATOMIC_RELAXED);
        return 1;
    }
    if(!bake_native(k, z, x, y, TS, out, n)) return 0;
    __atomic_fetch_add(k == FB_ALBEDO_OSM ? &g_native_bakes : &g_bakes, 1, __ATOMIC_RELAXED);
    return 1;
}

void fb_bake_stats(long *hits, long *bakes, long *fails){
    if(hits) *hits = __atomic_load_n(&g_hits, __ATOMIC_RELAXED);
    if(bakes) *bakes = __atomic_load_n(&g_native_bakes, __ATOMIC_RELAXED)
                      + __atomic_load_n(&g_super_bakes, __ATOMIC_RELAXED)
                      + __atomic_load_n(&g_bakes, __ATOMIC_RELAXED);
    if(fails) *fails = __atomic_load_n(&g_fail, __ATOMIC_RELAXED);
}

void fb_bake_stats2(long *native_bakes, long *super_bakes){
    if(native_bakes) *native_bakes = __atomic_load_n(&g_native_bakes, __ATOMIC_RELAXED);
    if(super_bakes)  *super_bakes  = __atomic_load_n(&g_super_bakes, __ATOMIC_RELAXED);
}
