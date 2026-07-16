/* FlightBox tiles — ground albedo baking. See raster.h for why this lives on the server.
 *
 * Moved here from the renderer's world3d.h, where it re-rasterised the same vector tiles on every
 * page load. The drawing code is unchanged in spirit; what changed is that its output is now kept.
 */
#define _GNU_SOURCE
#include "raster.h"
#include "style.h"
#include "draw.h"
#include "cache.h"
#include <osmmesh/osmmesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* stb_image: decode the aerial JPEGs. The implementation lives in osmmesh/src/terrain.c (one per
 * link); this is declarations only. */
#include "../geo/osmmesh/src/3rdparty/stb_image.h"

/* The rasteriser itself lives in draw.h: pure, so it can be unit-tested without a network and
 * without osmmesh's decoder linked in. What is left here is what genuinely needs the world --
 * the tile cache and the JPEG decode. */

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
    if(!fb_cache_get(FB_TILE_VECTOR, z, x, y, &d, &n)){
        /* ABSENT is not failure: upstream HAS no vector tile here, which means nothing is mapped --
         * and the base fill fb_raster_bake already laid down IS the albedo. Returning 0 made /bake
         * answer 202 forever over such ground (measured: 202/202/202, bake_fail=18).
         *
         * Do not read this as "ocean". Measured, and it inverts the obvious guess: the ocean HAS a
         * vector tile (Shortbread carries ocean as a polygon; z12 mid-Atlantic answers 200). The
         * holes are UNMAPPED LAND -- the one that proved this fix sits at -41.77/-66.01, which is
         * ~80 km inland in Rio Negro, Patagonian steppe with nothing in OSM and 545 m of Terrarium
         * elevation under it. So green is the right answer here, and blue would have been a
         * confident, plausible, measured-nowhere mistake. */
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

    /* Order is the map: landcover, then what sits on it, then what runs over that. Roads and
     * rivers are drawn LAST on purpose -- they are the thinnest features and the most read. */
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
            if(rail != pass) continue;              /* rails in a 2nd pass: they stay visible at crossings */
            fb_draw_line(im,TS,TS,ft,sc,w,r,g,b);
        }
    }
    if((L=layer_named(t,"water_lines"))){
        float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(L,f);
            if(ft->geom_type == OSMMESH_MVT_GEOM_LINESTRING)
                /* 3 tile-edge fractions, same unit as every width in style.h — the reference
                 * denominator lives there and only there, or the rivers and the roads drift
                 * apart the day someone tunes one of them. */
                fb_draw_line(im,TS,TS,ft,sc, 3.0f*(float)TS/FB_STYLE_REF_TEX, 92,140,190);
        }
    }
    osmmesh_mvt_free(t);
    return 1;
}

/* --- aerial photo mosaic ------------------------------------------------------------------ */

static int bake_photo(int z, long x, long y, int TS, uint8_t *im){
    int f  = fb_mosaic_factor(TS);
    int zi = fb_mosaic_zoom(z, TS);
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

long fb_raster_scanline_overflows(void){ return fb_draw_refused; }
