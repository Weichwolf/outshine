#ifndef FB_DRAW_H
#define FB_DRAW_H
#include "OsmVector.h"

#include <stdint.h>
#include <math.h>

#define FB_XS_MAX 4096

static long fb_draw_refused = 0;

static void fb_draw_px(uint8_t *im, int W, int H, int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H){
        uint8_t *p = im + ((size_t)y*W + x)*3; p[0]=r; p[1]=g; p[2]=b;
    }
}

/* v11: back to a hard (non-anti-aliased) rasteriser, on the user's explicit call -- v8/v9's span-
 * edge blend and v10's font-quality coverage rasteriser both measured negligible-to-strong visual
 * improvement at real, unavoidable cost (v9: measurable; v10: 44-107% over budget even after two
 * dedicated optimisation rounds, see bakepool/bake.c history), and the verdict was that raw bake
 * speed matters more than edge softness for this renderer. The STRUCTURAL wins from that whole
 * effort stay: native-resolution rasterisation (no supersampling, see bake.c), Feature-LOD, the
 * multi-threaded connection pool, blocking /bake, the nginx cache -- none of that was about AA. */
static void fb_draw_disk(uint8_t *im, int W, int H, float cx, float cy, float rad,
                         uint8_t r, uint8_t g, uint8_t b){
    int x0=(int)(cx-rad), x1=(int)(cx+rad), y0=(int)(cy-rad), y1=(int)(cy+rad);
    float rr = rad*rad;
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
        float dx=(float)x-cx, dy=(float)y-cy; if(dx*dx+dy*dy <= rr) fb_draw_px(im,W,H,x,y,r,g,b);
    }
}

static void fb_draw_thick(uint8_t *im, int W, int H, float x0, float y0, float x1, float y1,
                          float w, uint8_t r, uint8_t g, uint8_t b){
    float dx=x1-x0, dy=y1-y0, len=sqrtf(dx*dx+dy*dy);
    int n=(int)len+1; float rad=w*0.5f; if(rad<0.6f) rad=0.6f;
    for(int i=0;i<=n;i++){ float t=(float)i/n; fb_draw_disk(im,W,H,x0+dx*t,y0+dy*t,rad,r,g,b); }
}

static void fb_draw_fill(uint8_t *im, int W, int H, const outshine::World::OsmVector &mvt,
                         const outshine::World::OsmVector::Feature &ft, float sc,
                         uint8_t r, uint8_t g, uint8_t b){
    const int32_t *co = mvt.Points().data();
    const outshine::World::OsmVector::Ring *rings = mvt.Rings().data() + ft.FirstRing;
    const uint32_t nr = ft.RingCount;
    if(nr == 0) return;
    float ymin=1e9f, ymax=-1e9f;
    for(uint32_t ring=0; ring<nr; ring++)
        for(uint32_t k=0;k<rings[ring].Count;k++){
            float y=(float)co[((size_t)rings[ring].First+k)*2+1]*sc; if(y<ymin)ymin=y; if(y>ymax)ymax=y; }
    int iy0=(int)floorf(ymin); if(iy0<0) iy0=0;
    int iy1=(int)ceilf(ymax);  if(iy1>=H) iy1=H-1;
    /* NOT static: many worker threads bake concurrently (main.c's connection pool + prefetch.c),
     * each through this same function for a different tile's polygons -- a shared xs[] would let
     * one thread's crossings clobber another's mid-sort, corrupting the fill with a "large wrong-
     * colored span" (proven with a concurrent-vs-single-threaded stress test before the original
     * fix: 8/8 threads diverged from the sequential reference by millions of bytes every round).
     * 16 KB/call on the stack is fine, not a hot recursive path. */
    float xs[FB_XS_MAX];
    for(int y=iy0;y<=iy1;y++){
        float yc = y + 0.5f;
        int nx = 0, overflow = 0;
        for(uint32_t ring=0; ring<nr; ring++){
            size_t a  = rings[ring].First;
            size_t bb = a + rings[ring].Count;
            for(size_t k=a;k<bb;k++){
                size_t k2 = (k+1<bb) ? k+1 : a;
                float ya=(float)co[k*2+1]*sc,  yb=(float)co[k2*2+1]*sc;
                float xa=(float)co[k*2]*sc,    xb=(float)co[k2*2]*sc;

                if((ya<=yc && yb>yc) || (yb<=yc && ya>yc)){
                    float xi = xa + (yc-ya)/(yb-ya)*(xb-xa);
                    if(nx < FB_XS_MAX) xs[nx++] = xi; else overflow = 1;
                }
            }
        }
        if(overflow){ __atomic_fetch_add(&fb_draw_refused, 1, __ATOMIC_RELAXED); continue; }
        if(nx & 1){ __atomic_fetch_add(&fb_draw_refused, 1, __ATOMIC_RELAXED); continue; }
        for(int i=1;i<nx;i++){ float v=xs[i]; int j=i-1; while(j>=0 && xs[j]>v){ xs[j+1]=xs[j]; j--; } xs[j+1]=v; }
        for(int i=0;i+1<nx;i+=2){
            int xa=(int)ceilf(xs[i]-0.5f);    if(xa<0) xa=0;
            int xb=(int)floorf(xs[i+1]-0.5f); if(xb>=W) xb=W-1;
            for(int x=xa;x<=xb;x++) fb_draw_px(im,W,H,x,y,r,g,b);
        }
    }
}

static void fb_draw_line(uint8_t *im, int W, int H, const outshine::World::OsmVector &mvt,
                         const outshine::World::OsmVector::Feature &ft, float sc,
                         float w, uint8_t r, uint8_t g, uint8_t b){
    const int32_t *co = mvt.Points().data();
    const outshine::World::OsmVector::Ring *rings = mvt.Rings().data() + ft.FirstRing;
    for(uint32_t ring=0; ring<ft.RingCount; ring++){
        size_t a  = rings[ring].First;
        size_t bb = a + rings[ring].Count;
        for(size_t k=a+1;k<bb;k++)
            fb_draw_thick(im,W,H, (float)co[(k-1)*2]*sc, (float)co[(k-1)*2+1]*sc,
                          (float)co[k*2]*sc, (float)co[k*2+1]*sc, w, r,g,b);
    }
}

static int fb_mosaic_factor(int TS){ int f = TS/256; return f < 1 ? 1 : f; }
static int fb_mosaic_zoom(int z, int TS){
    int zi = z; for(int t = fb_mosaic_factor(TS); t > 1; t >>= 1) zi++;
    return zi;
}

#endif
