#ifndef FB_STYLE_H
#define FB_STYLE_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* Aviation kinds: in the 4 airports sampled (Grenchen/ZRH/EDDF/JFK), aerodrome/helipad only ever
 * showed up as POINTs (public_transport layer) and runway/taxiway as LINESTRINGs (streets layer) --
 * never as a "land" polygon, so these entries are currently dead code for THIS render path. Kept
 * because Shortbread's schema allows aerodrome/model_aerodrome/airstrip/runway/taxiway/apron/helipad
 * as polygon kinds too (some regions map an airfield boundary that way), and because the real bug
 * they were first suspected of (a runway-shaped stripe) turned out to be something else entirely:
 * an OSM multipolygon hole (see fb_raster_bake's background comment in raster.c) that this table
 * can't reach at all. Left styled defensively; the fallback-color decoupling below is the real fix. */
static long fb_style_unknown_hits = 0;
static const char *fb_style_unknown_seen[32];
static int fb_style_unknown_nseen = 0;
/* w3_landcolor runs on FB_PF_THREADS_DEFAULT concurrent bake workers (prefetch.c) -- same shared-
 * mutable-state hazard as fb_draw_fill's former static xs[] (draw.h), just for diagnostics instead
 * of pixels: an unguarded "nseen<32; seen[nseen++]=" is a real race between threads hitting a new
 * unstyled kind at once. Diagnostics only, but a data race is still a data race. */
static pthread_mutex_t fb_style_unknown_mx = PTHREAD_MUTEX_INITIALIZER;

static void w3_landcolor(const char*k,uint8_t*r,uint8_t*g,uint8_t*b){
  struct{const char*k;uint8_t r,g,bl;} T[]={
    {"wood",70,105,60},{"forest",70,105,60},{"scrub",125,150,85},{"heath",150,160,100},
    {"farmland",206,192,142},{"farmyard",192,178,132},{"allotments",182,186,122},{"vineyard",170,180,120},
    {"meadow",156,192,112},{"grass",162,196,116},{"grassland",162,196,116},{"park",142,192,122},
    {"garden",150,192,120},{"playground",150,190,120},{"cemetery",122,156,112},{"recreation_ground",150,190,120},
    {"residential",206,199,189},{"commercial",196,181,166},{"retail",202,182,162},{"industrial",176,166,176},
    {"quarry",180,170,160},{"sand",225,215,170},{"beach",235,225,180},
    {"aerodrome",158,188,114},{"model_aerodrome",156,184,112},{"airstrip",154,182,110},
    {"runway",168,166,172},{"taxiway",174,172,178},{"apron",178,176,182},{"helipad",172,168,176},

    /* Gate-Run-2 gap fix: 20 kinds real OSM data carries that had no entry (fell through to the
     * rose-grey debug fallback in production tiles). Colors follow OSM-Carto's standard palette
     * families, not invented -- bare rock/scree/shingle share one grey-beige (Carto renders all
     * three near-identically); the wetland foursome gets a muted green-teal distinct from the
     * vivid grass/meadow greens; orchard/plant_nursery are plantation greens visibly different
     * from forest's dark canopy tone; golf_course/village_green/greenfield/miniature_golf are
     * meadow-family greens (each a hair apart, not literal duplicates); railway (landuse, not the
     * rail LINESTRING already in w3_roadstyle) is infrastructure grey; brownfield/landfill are
     * brown-ochre; garages is built-up grey; grave_yard is a more desaturated cemetery green-grey. */
    {"bare_rock",190,185,175},{"scree",190,185,175},{"shingle",195,190,182},
    {"bog",145,178,160},{"marsh",140,172,162},{"swamp",128,162,150},{"string_bog",145,178,160},
    {"wet_meadow",152,186,148},
    {"orchard",140,175,90},{"plant_nursery",148,182,100},
    {"greenhouse_horticulture",205,213,200},
    {"golf_course",165,198,120},{"village_green",160,195,116},{"greenfield",158,193,114},
    {"miniature_golf",163,197,118},
    {"railway",214,213,213},
    {"brownfield",180,160,110},{"landfill",172,148,98},
    {"garages",195,190,185},
    {"grave_yard",136,150,130}};
  for(size_t i=0;i<sizeof(T)/sizeof(T[0]);i++) if(!strcmp(k,T[i].k)){*r=T[i].r;*g=T[i].g;*b=T[i].bl;return;}

  /* Deliberately NOT the background fill color: a debug-recognisable rose-grey, so the NEXT missing
   * style entry looks wrong instead of looking like missing data. Logged once per distinct kind
   * (not per hit -- a busy tile can carry hundreds of features of the same unstyled kind); the
   * running total is in /health so a style gap shows up even if nobody reads the log. */
  pthread_mutex_lock(&fb_style_unknown_mx);
  fb_style_unknown_hits++;
  int known = 0;
  for(int i=0;i<fb_style_unknown_nseen;i++) if(!strcmp(fb_style_unknown_seen[i],k)){ known = 1; break; }
  if(!known && fb_style_unknown_nseen < 32){
    fprintf(stderr, "[fb-tiles] w3_landcolor: unstyled land kind \"%s\" -> debug fallback color\n", k);
    fb_style_unknown_seen[fb_style_unknown_nseen++] = strdup(k);
  }
  pthread_mutex_unlock(&fb_style_unknown_mx);
  *r=190;*g=170;*b=170;
}

static long fb_style_unknown(void){
    pthread_mutex_lock(&fb_style_unknown_mx);
    long n = fb_style_unknown_hits;
    pthread_mutex_unlock(&fb_style_unknown_mx);
    return n;
}

#define FB_STYLE_REF_TEX 1024.0f

static float w3_roadstyle(const char*k,int tex_res,uint8_t*r,uint8_t*g,uint8_t*b,int*rail){
  *rail=0; float u=(float)tex_res/FB_STYLE_REF_TEX;
  if(!strcmp(k,"rail")||!strcmp(k,"tram")){*r=95;*g=95;*b=105;*rail=1;return 2.0f*u;}
  if(!strcmp(k,"motorway")||!strcmp(k,"trunk")){*r=250;*g=205;*b=140;return 6*u;}
  if(!strcmp(k,"primary")){*r=250;*g=222;*b=165;return 5*u;}
  if(!strcmp(k,"secondary")){*r=250;*g=242;*b=205;return 4*u;}
  if(!strcmp(k,"tertiary")){*r=246;*g=242;*b=222;return 3.2f*u;}
  if(!strcmp(k,"residential")||!strcmp(k,"living_street")||!strcmp(k,"unclassified")||!strcmp(k,"service")){*r=236;*g=233;*b=225;return 2.4f*u;}
  *r=200;*g=175;*b=140;return 1.4f*u;
}

#endif
