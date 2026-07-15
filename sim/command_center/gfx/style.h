/* FlightBox renderer — OSM cartography: what colour is a thing, how wide is a road.
 *
 * Split out of world3d.h because it is pure data-driven mapping: a Shortbread "kind" string in,
 * an RGB triple (and a line width) out. No GL, no osmmesh types, no state — so the map's look is
 * assertable in a unit test instead of only judgeable by staring at a baked tile.
 *
 * The palette is deliberately flat/unlit: the tile texture is ALBEDO, and the sun is applied
 * per-pixel by the terrain shader. Baking shadows in here would double-light the world.
 */
#ifndef W3_STYLE_H
#define W3_STYLE_H
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

/* Street "kind" -> colour + stroke width in texture pixels, and whether it is a railway.
 * Rails are drawn in a second pass so they stay visible where they cross roads.
 *
 * tex_res = the resolution of the texture being baked. Widths are tuned for a ~1.5 km tile at
 * 1024 px, so they scale with it: a 256 px far tile must get proportionally thinner roads, not
 * the same pixel widths (which would render as a coarse grey mat at distance).
 */
static float w3_roadstyle(const char*k,int tex_res,uint8_t*r,uint8_t*g,uint8_t*b,int*rail){
  *rail=0; float u=(float)tex_res/1024.0f;
  if(!strcmp(k,"rail")||!strcmp(k,"tram")){*r=95;*g=95;*b=105;*rail=1;return 2.0f*u;}
  if(!strcmp(k,"motorway")||!strcmp(k,"trunk")){*r=250;*g=205;*b=140;return 6*u;}
  if(!strcmp(k,"primary")){*r=250;*g=222;*b=165;return 5*u;}
  if(!strcmp(k,"secondary")){*r=250;*g=242;*b=205;return 4*u;}
  if(!strcmp(k,"tertiary")){*r=246;*g=242;*b=222;return 3.2f*u;}
  if(!strcmp(k,"residential")||!strcmp(k,"living_street")||!strcmp(k,"unclassified")||!strcmp(k,"service")){*r=236;*g=233;*b=225;return 2.4f*u;}
  *r=200;*g=175;*b=140;return 1.4f*u;    /* track/path/footway/steps */
}

#endif /* W3_STYLE_H */
