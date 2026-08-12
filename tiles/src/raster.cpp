#include "raster.h"
#include "style.h"
#include "draw.h"
#include "cache.h"
#include "tilemap_api.h"
#include "OsmVector.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

using outshine::World::OsmVector;

/* The style tables take a C string; the reader's view is not terminated, so the copy is where the
 * two conventions meet and it is bounded by the longest kind any table names. */
static void kind_of(const OsmVector &mvt, const OsmVector::Feature &f, char *out, size_t cap){
    const std::string_view kind = mvt.Str(f, "kind");
    const size_t n = kind.size() < cap - 1 ? kind.size() : cap - 1;
    memcpy(out, kind.data(), n);
    out[n] = 0;
}

/* ---- Feature-LOD: the actual speed lever -------------------------------------------------------
 * Fill cost scales with polygon count and rasterised line length, not with raster resolution alone
 * -- a distant/small-TS tile still decodes and would still fill every one of a city's thousands of
 * building footprints and driveways unless something skips them first. Both gates key off lod_ts
 * (the SERVED size), not TS (which may be a supersampled native raster larger than what's served):
 * a feature invisible at the size the client actually gets is invisible regardless of how large the
 * intermediate raster is. */
#define FB_LOD_MIN_AREA_PX 1.5f

/* Bounding-box area, not exact polygon area: cheap (one pass over the coords we're already about to
 * touch), and a conservative over-estimate (a rotated/thin polygon's AABB is >= its true area) --
 * the failure direction of that approximation is "occasionally keep a feature slightly too small to
 * matter", never "drop something that would have been visible". sc_lod converts raw MVT coords
 * directly to SERVED pixels (see call sites: sc_lod = sc * lod_ts/TS). */
static float fb_feature_bbox_px(const OsmVector &mvt, const OsmVector::Feature &ft, float sc_lod){
    if(ft.RingCount == 0) return 1e9f;   /* nothing to measure: never skip blind */
    const int32_t *co = mvt.Points().data();
    const OsmVector::Ring *rings = mvt.Rings().data() + ft.FirstRing;
    float x0=1e9f, x1=-1e9f, y0=1e9f, y1=-1e9f;
    for(uint32_t ring=0; ring<ft.RingCount; ring++)
        for(uint32_t k=0;k<rings[ring].Count;k++){
            float x=(float)co[((size_t)rings[ring].First+k)*2];
            float y=(float)co[((size_t)rings[ring].First+k)*2+1];
            if(x<x0) x0=x;
            if(x>x1) x1=x;
            if(y<y0) y0=y;
            if(y>y1) y1=y;
        }
    return (x1-x0)*sc_lod * (y1-y0)*sc_lod;
}

/* Per-class visibility floor for LINESTRING streets, tuned like w3_roadstyle/w3_landcolor (a hand-
 * picked cartographic choice, not a physical constant): motorway/trunk/primary/rail/tram always
 * render (navigation-critical or rare enough that their fill cost is negligible either way);
 * secondary/tertiary/residential-tier roads need a moderately detailed serve size before they're
 * worth their fill cost; the numerous small stuff (service/track/path/footway/steps/cycleway/...)
 * only appears once the target is genuinely close-up. Thresholds are on lod_ts, the SAME axis a
 * real slippy map generalises on, so 256->512->1024 reveals progressively more detail the way a
 * zoom level naturally would -- not a jarring pop-in (checked against neighbour-LOD screenshots,
 * not just asserted). */
static int fb_lod_line_ok(const char *kind, int lod_ts){
    if(!strcmp(kind,"motorway") || !strcmp(kind,"trunk") || !strcmp(kind,"primary")
       || !strcmp(kind,"rail")  || !strcmp(kind,"tram")) return 1;
    if(!strcmp(kind,"secondary")) return lod_ts >= 256;
    if(!strcmp(kind,"tertiary") || !strcmp(kind,"residential")
       || !strcmp(kind,"living_street") || !strcmp(kind,"unclassified")) return lod_ts >= 512;
    return lod_ts >= 1024;   /* service/track/path/footway/steps/cycleway/... */
}

static int bake_osm(int z, long x, long y, int TS, int lod_ts, uint8_t *im){
    uint8_t *d = 0; size_t n = 0;
    if(!fb_cache_get(FB_TILE_VECTOR, z, x, y, &d, &n)){

        uint8_t *p = 0; size_t pn = 0;
        int absent = fb_cache_state(FB_TILE_VECTOR, z, x, y, &p, &pn) == FB_TILE_ABSENT;
        free(p);
        return absent;
    }
    OsmVector mvt;
    uint8_t r,g,b; int rail;
    char kind[64];
    bool parsed = false;
    /* ONE LAYER PER PASS, and the reader walks the tile once for each: the layers are drawn in a
     * fixed order anyway, so a whole-tile tree would buy nothing this loop does not already have. */
    #define EACH_POLY(name) \
        if(bool found_ = mvt.Parse(d, n, name)){ parsed |= found_; \
          float sc=(float)TS/(float)mvt.Extent(); \
          float sc_lod = sc*((float)lod_ts/(float)TS); \
          for(const OsmVector::Feature &ft : mvt.Features()){ \
            if(ft.Type != 3) continue; \
            if(fb_feature_bbox_px(mvt,ft,sc_lod) < FB_LOD_MIN_AREA_PX) continue;

    /* OCEAN FIRST, und es ist eine echte Ebene der Quelle, keine Heuristik. [MESS] die Vektorkachel
     * z7/49/48 (offener Atlantik) traegt genau eine Ebene, und sie heisst "ocean"; z7/66/45 (Schweiz)
     * traegt land/water_polygons/streets und KEIN ocean. Sie wurde hier nie abgefragt, weshalb jede
     * Seekachel als reines Vorfuell-Beige herauskam — gemessen 100 % (235,231,221) auf Atlantik UND
     * Nordsee. Dieselbe Blau-Konstante wie water_polygons: ein Gewaesser hat eine Farbe, nicht zwei.
     * Vor land gezeichnet, damit an der Kueste das Land obenauf liegt. */
    EACH_POLY("ocean")           fb_draw_fill(im,TS,TS,mvt,ft,sc, 92,140,190); } }
    EACH_POLY("land")            kind_of(mvt,ft,kind,sizeof kind); w3_landcolor(kind,&r,&g,&b);
                                 fb_draw_fill(im,TS,TS,mvt,ft,sc,r,g,b); } }
    EACH_POLY("water_polygons")  fb_draw_fill(im,TS,TS,mvt,ft,sc, 92,140,190); } }
    EACH_POLY("sites")           fb_draw_fill(im,TS,TS,mvt,ft,sc,175,175,180); } }
    EACH_POLY("street_polygons") fb_draw_fill(im,TS,TS,mvt,ft,sc,208,203,193); } }
    EACH_POLY("buildings")       fb_draw_fill(im,TS,TS,mvt,ft,sc,128,114,102); } }
    #undef EACH_POLY

    if(mvt.Parse(d, n, "streets")){
        parsed = true;
        float sc=(float)TS/(float)mvt.Extent();
        for(int pass=0;pass<2;pass++) for(const OsmVector::Feature &ft : mvt.Features()){
            if(ft.Type != 2) continue;
            kind_of(mvt,ft,kind,sizeof kind);
            if(!fb_lod_line_ok(kind, lod_ts)) continue;
            float w = w3_roadstyle(kind, TS, &r,&g,&b, &rail);
            if(rail != pass) continue;
            fb_draw_line(im,TS,TS,mvt,ft,sc,w,r,g,b);
        }
    }
    if(mvt.Parse(d, n, "water_lines")){
        parsed = true;
        float sc=(float)TS/(float)mvt.Extent();
        for(const OsmVector::Feature &ft : mvt.Features())
            if(ft.Type == 2)
                fb_draw_line(im,TS,TS,mvt,ft,sc, 3.0f*(float)TS/FB_STYLE_REF_TEX, 92,140,190);
    }
    free(d);
    /* NOT ONE LAYER READ is bytes that are not a vector tile, and a beige square is the wrong answer
     * to that: the caller must be free to read it as a defect. */
    return parsed ? 1 : 0;
}

static int photo_zoom(int z, long x, long y, int zi){
    for(int zs = zi; zs > z; zs--){
        int fs = 1 << (zs - z);

        for(int j = 0; j < fs; j++) for(int i = 0; i < fs; i++)
            if(fb_tm_has(zs, x*(long)fs + i, y*(long)fs + j) != 0) return zs;
    }
    return z;
}

static int bake_photo(int z, long x, long y, int TS, uint8_t *im){
    int zi = fb_mosaic_zoom(z, TS);
    int zs = photo_zoom(z, x, y, zi);
    int fs = 1 << (zs - z);
    int span = TS / fs;
    if(span < 1) return 0;
    int got = 0;
    for(int j=0;j<fs;j++) for(int i=0;i<fs;i++){
        uint8_t *b = 0; size_t n = 0;
        if(!fb_cache_get(FB_TILE_IMAGERY, zs, x*(long)fs + i, y*(long)fs + j, &b, &n)) continue;
        SDL_IOStream *io = SDL_IOFromConstMem(b, n);
        SDL_Surface *decoded = io ? IMG_Load_IO(io, true) : 0;
        free(b);
        if(!decoded) continue;
        SDL_Surface *rgb24 = SDL_ConvertSurface(decoded, SDL_PIXELFORMAT_RGB24);
        SDL_DestroySurface(decoded);
        if(!rgb24) continue;
        const int w = rgb24->w, h = rgb24->h;
        const uint8_t *p = (const uint8_t *)rgb24->pixels;

        for(int dy = 0; dy < span; dy++){
            int oy = j*span + dy; if(oy >= TS) break;
            int sy = dy * h / span;
            for(int dx = 0; dx < span; dx++){
                int ox = i*span + dx; if(ox >= TS) break;
                int sx = dx * w / span;
                uint8_t *dst = im + ((size_t)oy*TS + ox)*3;
                const uint8_t *src = p + (size_t)sy*(size_t)rgb24->pitch + (size_t)sx*3;
                dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2];
            }
        }
        SDL_DestroySurface(rgb24);
        got++;
    }
    return got > 0;
}

/* Pre-fill for whatever fb_draw_fill never touches: NOT plausible ground. It used to be a green
 * (150,178,118) that read as "probably grass" -- fine for a genuinely unmapped patch, but OSM
 * multipolygons routinely cut real holes into a landuse fill (e.g. landuse=grass around an airfield
 * with the paved runway/taxiway/apron excluded as an inner ring, since pavement isn't grass). Those
 * holes are legitimate cartography, not missing data, but a green pre-fill right next to the real
 * "grass" style made them look like a second, wrong landuse -- a runway-shaped stripe. Overridden to
 * a neutral, cartography-standard beige (OSM-Carto's background, ~#F2EFE9 toned to this palette):
 * unmapped ground reads as plain, unremarkable terrain, and a paved-surface hole reads as pavement,
 * not as a competing and wrong landuse. w3_landcolor's OWN fallback (style.h, for a genuinely
 * unrecognised "kind" string) keeps the separate rose-grey debug tone -- that one SHOULD stand out,
 * this one should not. */
int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, int lod_ts, uint8_t *rgb){
    for(int i=0;i<TS*TS;i++){ rgb[i*3]=235; rgb[i*3+1]=231; rgb[i*3+2]=221; }
    /* bake_photo resamples real aerial photography (fb_tm_has/z16 mosaic tiles); its own nearest-
     * neighbour resample (bake_photo above) is the analogous minification step for that path, but a
     * photo has no thin single-texel vector edges to alias into stripes the way rasterised OSM lines
     * do, and it already zooms its SOURCE tiles up with fb_mosaic_zoom for a requested TS rather than
     * rasterising fresh at low native res -- checked, not extended: it doesn't have this bug, and it
     * has no vector features to LOD-drop, so lod_ts is unused on this path. */
    return (kind==FB_ALBEDO_PHOTO) ? bake_photo(z,x,y,TS,rgb) : bake_osm(z,x,y,TS,lod_ts,rgb);
}

long fb_raster_scanline_overflows(void){ return __atomic_load_n(&fb_draw_refused, __ATOMIC_RELAXED); }
long fb_style_unknown_count(void){ return fb_style_unknown(); }

/* ---- linear-correct box downsample --------------------------------------------------------------
 * fb_draw_fill/fb_draw_line have no anti-aliasing of their own (a hard scanline/stamp fill), so a
 * requested TS rasterised NATIVELY at that exact size would show jagged, stairstepped, near-parallel
 * artifacts on field-boundary/road geometry that looks smooth at higher detail -- exactly the
 * "parallel stripes at low detail" a moire researcher would recognise. bake.c's answer (2026-07-23,
 * replacing the always-2048-master approach that this comment used to describe): rasterise at 2x the
 * SERVED size and do ONE box-halving here -- real 4x supersampling AA, at 1/16th the fill cost of a
 * fixed 2048 master for a 256 request, since fill cost scales with raster AREA. Every output texel is
 * a true area average of a 2x2 block of the properly-detailed native raster.
 *
 * Same construction as the client's mip builder (FBRenderer.cpp BuildMipsSrgb): average in LINEAR
 * light, not sRGB -- a gamma-naive average darkens the result (mid-tones average brighter in linear
 * than their sRGB midpoint), which would make the downscaled output visibly duller than its native
 * raster. */
static float fb_srgb_lin[256];
static pthread_once_t fb_srgb_lut_once = PTHREAD_ONCE_INIT;
static void fb_srgb_lut_build(void){
    for(int i=0;i<256;i++){
        float s = i/255.0f;
        fb_srgb_lin[i] = s <= 0.04045f ? s/12.92f : powf((s+0.055f)/1.055f, 2.4f);
    }
}
/* pthread_once, not a check-then-set flag: fb_bake_get's downscale path runs on
 * FB_PF_THREADS_DEFAULT prefetch workers (prefetch.c) -- a racy "if(ready) return; ...; ready=1"
 * lets a second thread read fb_srgb_lin mid-fill, same class of bug as fb_draw_fill's former
 * static xs[] (draw.h). */
static void fb_srgb_lut_init(void){ pthread_once(&fb_srgb_lut_once, fb_srgb_lut_build); }
static uint8_t fb_lin2srgb(float l){
    l = l <= 0.0031308f ? l*12.92f : 1.055f*powf(l, 1.0f/2.4f) - 0.055f;
    int v = (int)(l*255.0f + 0.5f);
    return (uint8_t)(v<0 ? 0 : v>255 ? 255 : v);
}

static void fb_downsample_step(const uint8_t *src, int w, uint8_t *dst){
    int nw = w/2;
    for(int y=0;y<nw;y++) for(int x=0;x<nw;x++){
        const uint8_t *a = src + ((size_t)(2*y)*w + 2*x)*3;
        const uint8_t *b = a+3;
        const uint8_t *c = src + ((size_t)(2*y+1)*w + 2*x)*3;
        const uint8_t *d = c+3;
        uint8_t *o = dst + ((size_t)y*nw+x)*3;
        for(int ch=0;ch<3;ch++)
            o[ch] = fb_lin2srgb(0.25f*(fb_srgb_lin[a[ch]]+fb_srgb_lin[b[ch]]+fb_srgb_lin[c[ch]]+fb_srgb_lin[d[ch]]));
    }
}

/* srcTS -> dstTS (dstTS <= srcTS, both powers of two) via repeated 2x box halving. `dst` must hold
 * dstTS*dstTS*3 bytes; `src` is read-only and untouched. */
int fb_raster_downscale(const uint8_t *src, int srcTS, uint8_t *dst, int dstTS){
    if(srcTS==dstTS){ memcpy(dst, src, (size_t)srcTS*srcTS*3); return 1; }
    if(dstTS<=0 || srcTS<=dstTS || srcTS % dstTS) return 0;
    int ratio = srcTS/dstTS;
    if(ratio & (ratio-1)) return 0;   /* ratio must itself be a power of two */
    fb_srgb_lut_init();

    uint8_t * cur = (uint8_t *)malloc((size_t)srcTS*srcTS*3);
    if(!cur) return 0;
    memcpy(cur, src, (size_t)srcTS*srcTS*3);
    int w = srcTS;
    while(w > dstTS){
        int nw = w/2;
        uint8_t *nxt = (nw==dstTS) ? dst : (uint8_t *)malloc((size_t)nw*nw*3);
        if(!nxt){ free(cur); return 0; }
        fb_downsample_step(cur, w, nxt);
        free(cur);
        cur = nxt;
        w = nw;
    }
    return 1;
}
