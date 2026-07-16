#define _GNU_SOURCE
#include "raster.h"
#include "style.h"
#include "draw.h"
#include "cache.h"
#include "tilemap_api.h"
#include <osmmesh/osmmesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../geo/osmmesh/src/3rdparty/stb_image.h"

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

static int bake_osm(int z, long x, long y, int TS, uint8_t *im){
    uint8_t *d = 0; size_t n = 0;
    if(!fb_cache_get(FB_TILE_VECTOR, z, x, y, &d, &n)){

        uint8_t *p = 0; size_t pn = 0;
        int absent = fb_cache_state(FB_TILE_VECTOR, z, x, y, &p, &pn) == FB_TILE_ABSENT;
        free(p);
        return absent;
    }
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

    EACH_POLY("land")            w3_landcolor(kind_of(L,ft),&r,&g,&b); fb_draw_fill(im,TS,TS,ft,sc,r,g,b); } }
    EACH_POLY("water_polygons")  fb_draw_fill(im,TS,TS,ft,sc, 92,140,190); } }
    EACH_POLY("sites")           fb_draw_fill(im,TS,TS,ft,sc,175,175,180); } }
    EACH_POLY("street_polygons") fb_draw_fill(im,TS,TS,ft,sc,208,203,193); } }
    EACH_POLY("buildings")       fb_draw_fill(im,TS,TS,ft,sc,128,114,102); } }
    #undef EACH_POLY

    if((L=layer_named(t,"streets"))){
        float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(int pass=0;pass<2;pass++) for(size_t f=0;f<nf;f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(L,f);
            if(ft->geom_type != OSMMESH_MVT_GEOM_LINESTRING) continue;
            float w = w3_roadstyle(kind_of(L,ft), TS, &r,&g,&b, &rail);
            if(rail != pass) continue;
            fb_draw_line(im,TS,TS,ft,sc,w,r,g,b);
        }
    }
    if((L=layer_named(t,"water_lines"))){
        float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(L,f);
            if(ft->geom_type == OSMMESH_MVT_GEOM_LINESTRING)

                fb_draw_line(im,TS,TS,ft,sc, 3.0f*(float)TS/FB_STYLE_REF_TEX, 92,140,190);
        }
    }
    osmmesh_mvt_free(t);
    return 1;
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
        int w=0,h=0,comp=0;
        uint8_t *p = stbi_load_from_memory(b, (int)n, &w, &h, &comp, 3);
        free(b);
        if(!p) continue;

        for(int dy = 0; dy < span; dy++){
            int oy = j*span + dy; if(oy >= TS) break;
            int sy = dy * h / span;
            for(int dx = 0; dx < span; dx++){
                int ox = i*span + dx; if(ox >= TS) break;
                int sx = dx * w / span;
                uint8_t *dst = im + ((size_t)oy*TS + ox)*3, *src = p + ((size_t)sy*w + sx)*3;
                dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2];
            }
        }
        stbi_image_free(p);
        got++;
    }
    return got > 0;
}

int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, uint8_t *rgb){
    for(int i=0;i<TS*TS;i++){ rgb[i*3]=150; rgb[i*3+1]=178; rgb[i*3+2]=118; }
    return (kind==FB_ALBEDO_PHOTO) ? bake_photo(z,x,y,TS,rgb) : bake_osm(z,x,y,TS,rgb);
}

long fb_raster_scanline_overflows(void){ return fb_draw_refused; }
