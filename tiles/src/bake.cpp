#include "bake.h"
#include "raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

typedef struct { uint8_t *b; size_t n; } membuf;

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

/* WHAT THE ENCODER COSTS WAS THE WHOLE BAKE. Stage-profiled at tex=4096: fill+lines ~0.13 s while
 * the PNG encode took 0.7-2.1 s on the same pixels, which is why stb_image_write's filter and
 * compression knobs used to be forced here. SDL3_image exposes neither, and it does not need to --
 * measured on a z14 Bern bake, IMG_SavePNG writes FEWER bytes than the tuned stb path in comparable
 * time (the numbers are in the round's report, not in this comment, because they decay). The format
 * itself is the open question: PNG was chosen so a bake could travel over HTTP to a browser, and
 * that wire is being deleted. */
int fb_bake_init(const char *dir){
    if(dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    char sub[320];
    snprintf(sub, sizeof sub, "%s/bake_osm",   g_dir); mkdir(sub, 0755);
    snprintf(sub, sizeof sub, "%s/bake_photo", g_dir); mkdir(sub, 0755);
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
    uint8_t * b = (uint8_t *)malloc((size_t)sz);
    if(!b || fread(b, 1, (size_t)sz, f) != (size_t)sz){ free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
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
    SDL_Surface *surface = SDL_CreateSurfaceFrom(TS, TS, SDL_PIXELFORMAT_RGB24, (void*)rgb, TS*3);
    SDL_IOStream *io = surface ? SDL_IOFromDynamicMem() : 0;
    bool wrote = false;
    if(io) wrote = (k == FB_ALBEDO_PHOTO) ? IMG_SaveJPG_IO(surface, io, false, 88)
                                          : IMG_SavePNG_IO(surface, io, false);
    if(surface) SDL_DestroySurface(surface);
    membuf m = {};
    if(wrote){
        const Sint64 size = SDL_GetIOSize(io);
        if(size > 0 && SDL_SeekIO(io, 0, SDL_IO_SEEK_SET) == 0){
            m.b = (uint8_t *)malloc((size_t)size);
            if(m.b && SDL_ReadIO(io, m.b, (size_t)size) == (size_t)size) m.n = (size_t)size;
            else { free(m.b); m.b = 0; }
        }
    }
    if(io) SDL_CloseIO(io);
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
    uint8_t * rgb = (uint8_t *)malloc((size_t)TS*TS*3);
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
    uint8_t * raw = (uint8_t *)malloc((size_t)native*native*3);
    if(!raw){ __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }
    if(!fb_raster_bake(k, z, x, y, native, TS, raw)){
        free(raw); __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0;
    }

    uint8_t * rgb = (uint8_t *)malloc((size_t)TS*TS*3);
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
