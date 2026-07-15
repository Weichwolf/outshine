/* FlightBox tiles — the software rasteriser: MVT geometry -> RGB pixels. Pure.
 *
 * Split out of raster.c so it can be asserted without a network and without linking osmmesh's
 * decoder: it touches only an RGB buffer and an osmmesh_mvt_feature, which is a plain struct a
 * test can build by hand. raster.c keeps the parts that need the world (cache, JPEG decode).
 *
 * Worth testing rather than eyeballing, because a scanline fill fails in a very specific and very
 * loud way: lose one crossing and the remaining ones pair up shifted, so "inside" and "outside"
 * swap and the fill runs to the next crossing — right across the tile if there isn't one. That is
 * not a subtle artifact, it is a solid rectangle over the map. It happened, over a power station.
 */
#ifndef FB_DRAW_H
#define FB_DRAW_H
#include <osmmesh/osmmesh.h>
#include <stdint.h>
#include <math.h>

/* Crossings-per-scanline limit. See fb_draw_fill: overflowing it is the one way this code can
 * paint a lie, so it is counted and the scanline is refused rather than guessed. */
#define FB_XS_MAX 4096

/* Scanlines refused: overflowed or came out odd (a degenerate ring). Should stay 0. Exposed so
 * /health can show it — a silent guess is what we are avoiding. */
static long fb_draw_refused = 0;

static void fb_draw_px(uint8_t *im, int W, int H, int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H){
        uint8_t *p = im + ((size_t)y*W + x)*3; p[0]=r; p[1]=g; p[2]=b;
    }
}

static void fb_draw_disk(uint8_t *im, int W, int H, float cx, float cy, float rad,
                         uint8_t r, uint8_t g, uint8_t b){
    int x0=(int)(cx-rad), x1=(int)(cx+rad), y0=(int)(cy-rad), y1=(int)(cy+rad);
    float rr = rad*rad;
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
        float dx=x-cx, dy=y-cy; if(dx*dx+dy*dy <= rr) fb_draw_px(im,W,H,x,y,r,g,b);
    }
}

/* A thick line as a swept disk: round caps and joins for free, which is what roads want. */
static void fb_draw_thick(uint8_t *im, int W, int H, float x0, float y0, float x1, float y1,
                          float w, uint8_t r, uint8_t g, uint8_t b){
    float dx=x1-x0, dy=y1-y0, len=sqrtf(dx*dx+dy*dy);
    int n=(int)len+1; float rad=w*0.5f; if(rad<0.6f) rad=0.6f;   /* never thinner than a pixel */
    for(int i=0;i<=n;i++){ float t=(float)i/n; fb_draw_disk(im,W,H,x0+dx*t,y0+dy*t,rad,r,g,b); }
}

/* Scanline fill of an MVT polygon: all rings, even-odd, so interior rings carve holes. */
static void fb_draw_fill(uint8_t *im, int W, int H, const osmmesh_mvt_feature *ft, float sc,
                         uint8_t r, uint8_t g, uint8_t b){
    const osmmesh_mvt_coord *co = ft->coords;
    size_t nco = ft->n_coords;
    if(nco < 3 || !co) return;
    float ymin=1e9f, ymax=-1e9f;
    for(size_t i=0;i<nco;i++){ float y=co[i].y*sc; if(y<ymin)ymin=y; if(y>ymax)ymax=y; }
    int iy0=(int)floorf(ymin); if(iy0<0) iy0=0;
    int iy1=(int)ceilf(ymax);  if(iy1>=H) iy1=H-1;
    size_t nr = ft->n_rings ? ft->n_rings : 1;
    static float xs[FB_XS_MAX];
    for(int y=iy0;y<=iy1;y++){
        float yc = y + 0.5f;
        int nx = 0, overflow = 0;
        for(size_t ring=0; ring<nr; ring++){
            size_t a  = ft->n_rings ? ft->ring_offsets[ring]   : 0;
            size_t bb = ft->n_rings ? ft->ring_offsets[ring+1] : nco;
            for(size_t k=a;k<bb;k++){
                size_t k2 = (k+1<bb) ? k+1 : a;         /* close the ring */
                float ya=co[k].y*sc,  yb=co[k2].y*sc;
                float xa=co[k].x*sc,  xb=co[k2].x*sc;
                /* half-open in y: a vertex exactly on the scanline counts once, not twice */
                if((ya<=yc && yb>yc) || (yb<=yc && ya>yc)){
                    float xi = xa + (yc-ya)/(yb-ya)*(xb-xa);
                    if(nx < FB_XS_MAX) xs[nx++] = xi; else overflow = 1;
                }
            }
        }
        if(overflow){ fb_draw_refused++; continue; }   /* skip, never guess */
        if(nx & 1){ fb_draw_refused++; continue; }     /* odd = degenerate ring; pairing would smear */
        for(int i=1;i<nx;i++){ float v=xs[i]; int j=i-1; while(j>=0 && xs[j]>v){ xs[j+1]=xs[j]; j--; } xs[j+1]=v; }
        for(int i=0;i+1<nx;i+=2){
            int xa=(int)ceilf(xs[i]-0.5f);    if(xa<0) xa=0;
            int xb=(int)floorf(xs[i+1]-0.5f); if(xb>=W) xb=W-1;
            for(int x=xa;x<=xb;x++) fb_draw_px(im,W,H,x,y,r,g,b);
        }
    }
}

static void fb_draw_line(uint8_t *im, int W, int H, const osmmesh_mvt_feature *ft, float sc,
                         float w, uint8_t r, uint8_t g, uint8_t b){
    const osmmesh_mvt_coord *co = ft->coords;
    if(!co || ft->n_coords < 2) return;
    size_t nr = ft->n_rings ? ft->n_rings : 1;
    for(size_t ring=0; ring<nr; ring++){
        size_t a  = ft->n_rings ? ft->ring_offsets[ring]   : 0;
        size_t bb = ft->n_rings ? ft->ring_offsets[ring+1] : ft->n_coords;
        for(size_t k=a+1;k<bb;k++)
            fb_draw_thick(im,W,H, co[k-1].x*sc, co[k-1].y*sc, co[k].x*sc, co[k].y*sc, w, r,g,b);
    }
}

/* --- aerial mosaic layout (pure arithmetic, no pixels) --------------------------------------
 * A photo tile is 256 px, so a TS-px texture is made of (TS/256)^2 children at z+log2(TS/256).
 * Exact, not a resample: z14 at 1024 -> 4x4 z16 children; z11 at 512 -> 2x2 z12. Getting this
 * wrong does not crash, it fetches the wrong ground. */
static int fb_mosaic_factor(int TS){ int f = TS/256; return f < 1 ? 1 : f; }
static int fb_mosaic_zoom(int z, int TS){
    int zi = z; for(int t = fb_mosaic_factor(TS); t > 1; t >>= 1) zi++;
    return zi;
}

#endif /* FB_DRAW_H */
