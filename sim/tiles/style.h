/* FlightBox — OSM cartography: what colour is a thing, how wide is a road.
 *
 * Pure data-driven mapping: a Shortbread "kind" string in, an RGB triple (and a line width) out.
 * No GL, no osmmesh types, no state — so the map's look is assertable in a unit test instead of
 * only judgeable by staring at a baked tile.
 *
 * Lives in tiles/ rather than command_center/ because that is where it now RUNS: fb-tiles bakes
 * the ground albedo and serves a finished texture, so the renderer no longer draws maps at all.
 *
 * The palette is deliberately flat/unlit: the tile texture is ALBEDO, and the sun is applied
 * per-pixel by the terrain shader. Baking shadows in here would double-light the world.
 */
#ifndef FB_STYLE_H
#define FB_STYLE_H
#include <stdint.h>
#include <string.h>

/* Landcover polygon -> base colour. Unknown kinds fall back to a neutral green:
 * an unstyled field should look like ground, never like a hole in the world. */
static void w3_landcolor(const char*k,uint8_t*r,uint8_t*g,uint8_t*b){
  struct{const char*k;uint8_t r,g,bl;} T[]={
    {"wood",70,105,60},{"forest",70,105,60},{"scrub",125,150,85},{"heath",150,160,100},
    {"farmland",206,192,142},{"farmyard",192,178,132},{"allotments",182,186,122},{"vineyard",170,180,120},
    {"meadow",156,192,112},{"grass",162,196,116},{"grassland",162,196,116},{"park",142,192,122},
    {"garden",150,192,120},{"playground",150,190,120},{"cemetery",122,156,112},{"recreation_ground",150,190,120},
    {"residential",206,199,189},{"commercial",196,181,166},{"retail",202,182,162},{"industrial",176,166,176},
    {"quarry",180,170,160},{"sand",225,215,170},{"beach",235,225,180}};
  for(size_t i=0;i<sizeof(T)/sizeof(T[0]);i++) if(!strcmp(k,T[i].k)){*r=T[i].r;*g=T[i].g;*b=T[i].bl;return;}
  *r=150;*g=178;*b=118;
}

/* The reference texture size the stroke widths below are expressed against. It is NOT a texture
 * size we bake at — it is the denominator of a UNIT. `6` means "6/1024 of the tile's edge", and
 * `u` converts that to whatever resolution is actually being baked.
 *
 * It has a name because it had none: the same 1024 was hand-inlined in raster.c for the river
 * stroke, so the two lived in separate files with no way to notice if one moved. Widths in
 * different units that all look like plain numbers is exactly how this project loses afternoons. */
#define FB_STYLE_REF_TEX 1024.0f

/* Street "kind" -> colour + stroke width, and whether it is a railway.
 * Rails are drawn in a second pass so they stay visible where they cross roads.
 *
 * THE UNIT IS TILE-EDGE FRACTIONS (1/FB_STYLE_REF_TEX of one edge), not pixels and not metres.
 * That distinction is not pedantry — it is the whole behaviour, and reading "pixels" here once
 * cost a wrong bug report:
 *   - across TEXTURE SIZE the ground width is CONSTANT (motorway = 8.8 m on a z14 tile, whether
 *     baked at 256 or 2048), because `u` cancels the resolution out. Measured, see test_style.c.
 *   - across ZOOM it scales with the tile's span, and that is deliberate cartographic
 *     generalisation: the same motorway is 564 m wide on a z8 tile. A z8 chunk is 96 km across and
 *     sits ~200 km away, so 564 m is about one screen pixel — at 8.8 m the road would simply not
 *     exist. Roads on a small-scale map are drawn wider than the ground truth, on purpose.
 */
static float w3_roadstyle(const char*k,int tex_res,uint8_t*r,uint8_t*g,uint8_t*b,int*rail){
  *rail=0; float u=(float)tex_res/FB_STYLE_REF_TEX;
  if(!strcmp(k,"rail")||!strcmp(k,"tram")){*r=95;*g=95;*b=105;*rail=1;return 2.0f*u;}
  if(!strcmp(k,"motorway")||!strcmp(k,"trunk")){*r=250;*g=205;*b=140;return 6*u;}
  if(!strcmp(k,"primary")){*r=250;*g=222;*b=165;return 5*u;}
  if(!strcmp(k,"secondary")){*r=250;*g=242;*b=205;return 4*u;}
  if(!strcmp(k,"tertiary")){*r=246;*g=242;*b=222;return 3.2f*u;}
  if(!strcmp(k,"residential")||!strcmp(k,"living_street")||!strcmp(k,"unclassified")||!strcmp(k,"service")){*r=236;*g=233;*b=225;return 2.4f*u;}
  *r=200;*g=175;*b=140;return 1.4f*u;    /* track/path/footway/steps */
}

#endif /* FB_STYLE_H */
