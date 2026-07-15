/* FlightBox tiles — ground albedo baking. See raster.h for why this lives on the server.
 *
 * Moved here from the renderer's world3d.h, where it re-rasterised the same vector tiles on every
 * page load. The drawing code is unchanged in spirit; what changed is that its output is now kept.
 */
#define _GNU_SOURCE
#include "raster.h"
#include "style.h"
#include "cache.h"
#include <osmmesh/osmmesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* stb_image: decode the aerial JPEGs. The implementation lives in osmmesh/src/terrain.c (one per
 * link); this is declarations only. */
#include "../geo/osmmesh/src/3rdparty/stb_image.h"

/* --- the software rasteriser ------------------------------------------------------------- */

static void px(uint8_t *im, int W, int H, int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H){
        uint8_t *p = im + ((size_t)y*W + x)*3; p[0]=r; p[1]=g; p[2]=b;
    }
}
static void disk(uint8_t *im, int W, int H, float cx, float cy, float rad, uint8_t r, uint8_t g, uint8_t b){
    int x0=(int)(cx-rad), x1=(int)(cx+rad), y0=(int)(cy-rad), y1=(int)(cy+rad);
    float rr = rad*rad;
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
        float dx=x-cx, dy=y-cy; if(dx*dx+dy*dy <= rr) px(im,W,H,x,y,r,g,b);
    }
}
static void thick(uint8_t *im, int W, int H, float x0, float y0, float x1, float y1, float w,
                  uint8_t r, uint8_t g, uint8_t b){
    float dx=x1-x0, dy=y1-y0, len=sqrtf(dx*dx+dy*dy);
    int n=(int)len+1; float rad=w*0.5f; if(rad<0.6f) rad=0.6f;
    for(int i=0;i<=n;i++){ float t=(float)i/n; disk(im,W,H,x0+dx*t,y0+dy*t,rad,r,g,b); }
}

/* Scanline fill of an MVT polygon: all rings, even-odd, so interior rings carve holes.
 *
 * FB_XS_MAX is the crossings-per-scanline limit. Overflowing it is the one way this function can
 * paint a lie: drop a crossing and the remaining ones pair up shifted, so "inside" and "outside"
 * swap and the fill runs to the next crossing — across the tile if there isn't one. That is not a
 * subtle artifact, it is a solid rectangle over whatever was already there. So we count it and
 * refuse to draw a scanline we know is wrong, rather than draw it wrong. */
#define FB_XS_MAX 4096
static long g_scanline_overflow = 0;

static void fill(uint8_t *im, int W, int H, const osmmesh_mvt_feature *ft, float sc,
                 uint8_t r, uint8_t g, uint8_t b){
    const osmmesh_mvt_coord *co = ft->coords;
    size_t nco = ft->n_coords;
    if(nco < 3) return;
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
                size_t k2 = (k+1<bb) ? k+1 : a;
                float ya=co[k].y*sc,  yb=co[k2].y*sc;
                float xa=co[k].x*sc,  xb=co[k2].x*sc;
                if((ya<=yc && yb>yc) || (yb<=yc && ya>yc)){
                    float xi = xa + (yc-ya)/(yb-ya)*(xb-xa);
                    if(nx < FB_XS_MAX) xs[nx++] = xi; else overflow = 1;
                }
            }
        }
        if(overflow){ g_scanline_overflow++; continue; }   /* skip, never guess */
        if(nx & 1){ g_scanline_overflow++; continue; }     /* odd = degenerate ring; pairing would smear */
        for(int i=1;i<nx;i++){ float v=xs[i]; int j=i-1; while(j>=0 && xs[j]>v){ xs[j+1]=xs[j]; j--; } xs[j+1]=v; }
        for(int i=0;i+1<nx;i+=2){
            int xa=(int)ceilf(xs[i]-0.5f);   if(xa<0) xa=0;
            int xb=(int)floorf(xs[i+1]-0.5f); if(xb>=W) xb=W-1;
            for(int x=xa;x<=xb;x++) px(im,W,H,x,y,r,g,b);
        }
    }
}

static void drawline(uint8_t *im, int W, int H, const osmmesh_mvt_feature *ft, float sc, float w,
                     uint8_t r, uint8_t g, uint8_t b){
    const osmmesh_mvt_coord *co = ft->coords;
    size_t nr = ft->n_rings ? ft->n_rings : 1;
    for(size_t ring=0; ring<nr; ring++){
        size_t a  = ft->n_rings ? ft->ring_offsets[ring]   : 0;
        size_t bb = ft->n_rings ? ft->ring_offsets[ring+1] : ft->n_coords;
        for(size_t k=a+1;k<bb;k++)
            thick(im,W,H, co[k-1].x*sc, co[k-1].y*sc, co[k].x*sc, co[k].y*sc, w, r,g,b);
    }
}

static const osmmesh_mvt_layer *layer_named(osmmesh_mvt_tile *t, const char *name){
    size_t nl = osmmesh_mvt_num_layers(t);
    for(size_t i=0;i<nl;i++){
        const osmmesh_mvt_layer *l = osmmesh_mvt_layer_at(t,i);
        if(!strcmp(osmmesh_mvt_layer_name(l), name)) return l;
    }
    return 0;
}
static const char *kind_of(const osmmesh_mvt_layer *l, const osmmesh_mvt_feature *f){
    osmmesh_mvt_value v;
    if(osmmesh_mvt_feature_get_tag(l,f,"kind",&v)==0 && v.type==OSMMESH_MVT_VAL_STRING) return v.v.s;
    return "";
}

/* --- OSM cartography ---------------------------------------------------------------------- */

static int bake_osm(int z, long x, long y, int TS, uint8_t *im){
    uint8_t *d = 0; size_t n = 0;
    if(!fb_cache_get(FB_TILE_VECTOR, z, x, y, &d, &n)) return 0;
    osmmesh_mvt_tile *t = 0;
    int rc = osmmesh_mvt_decode(d, n, &t);
    free(d);
    if(rc != OSMMESH_MVT_OK) return 0;

    const osmmesh_mvt_layer *L; uint8_t r,g,b; int rail;
    #define EACH_POLY(name) \
        if((L=layer_named(t,name))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); \
          size_t nf=osmmesh_mvt_num_features(L); \
          for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f); \
            if(ft->geom_type!=OSMMESH_MVT_GEOM_POLYGON) continue;

    /* Order is the map: landcover, then what sits on it, then what runs over that. Roads and
     * rivers are drawn LAST on purpose -- they are the thinnest features and the most read. */
    EACH_POLY("land")            w3_landcolor(kind_of(L,ft),&r,&g,&b); fill(im,TS,TS,ft,sc,r,g,b); } }
    EACH_POLY("water_polygons")  fill(im,TS,TS,ft,sc, 92,140,190); } }
    EACH_POLY("sites")           fill(im,TS,TS,ft,sc,175,175,180); } }
    EACH_POLY("street_polygons") fill(im,TS,TS,ft,sc,208,203,193); } }
    EACH_POLY("buildings")       fill(im,TS,TS,ft,sc,128,114,102); } }
    #undef EACH_POLY

    if((L=layer_named(t,"streets"))){
        float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(int pass=0;pass<2;pass++) for(size_t f=0;f<nf;f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(L,f);
            if(ft->geom_type != OSMMESH_MVT_GEOM_LINESTRING) continue;
            float w = w3_roadstyle(kind_of(L,ft), TS, &r,&g,&b, &rail);
            if(rail != pass) continue;              /* rails in a 2nd pass: they stay visible at crossings */
            drawline(im,TS,TS,ft,sc,w,r,g,b);
        }
    }
    if((L=layer_named(t,"water_lines"))){
        float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(L,f);
            if(ft->geom_type == OSMMESH_MVT_GEOM_LINESTRING)
                drawline(im,TS,TS,ft,sc, 3.0f*TS/1024.0f, 92,140,190);
        }
    }
    osmmesh_mvt_free(t);
    return 1;
}

/* --- aerial photo mosaic ------------------------------------------------------------------ */

static int bake_photo(int z, long x, long y, int TS, uint8_t *im){
    int f = TS/256; if(f < 1) f = 1;
    int zi = z; for(int t=f; t>1; t>>=1) zi++;
    int got = 0;
    for(int j=0;j<f;j++) for(int i=0;i<f;i++){
        uint8_t *b = 0; size_t n = 0;
        if(!fb_cache_get(FB_TILE_IMAGERY, zi, x*(long)f + i, y*(long)f + j, &b, &n)) continue;
        int w=0,h=0,comp=0;
        uint8_t *p = stbi_load_from_memory(b, (int)n, &w, &h, &comp, 3);
        free(b);
        if(!p) continue;
        for(int yy=0; yy<h; yy++){ int dy = j*256 + yy; if(dy >= TS) break;
            for(int xx=0; xx<w; xx++){ int dx = i*256 + xx; if(dx >= TS) break;
                uint8_t *dst = im + ((size_t)dy*TS + dx)*3, *src = p + ((size_t)yy*w + xx)*3;
                dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2];
            }
        }
        stbi_image_free(p);
        got++;
    }
    return got > 0;
}

int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, uint8_t *rgb){
    for(int i=0;i<TS*TS;i++){ rgb[i*3]=150; rgb[i*3+1]=178; rgb[i*3+2]=118; }   /* base ground */
    return (kind==FB_ALBEDO_PHOTO) ? bake_photo(z,x,y,TS,rgb) : bake_osm(z,x,y,TS,rgb);
}

long fb_raster_scanline_overflows(void){ return g_scanline_overflow; }
