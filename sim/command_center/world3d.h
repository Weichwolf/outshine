/* FlightBox 3D world + HUD renderer (GLES2/WebGL). Composition root: pulls in the sub-modules
 * (camera/atmo/stars/sky/terrain/tiles/hud) and owns the shared GL state + init. cc.c is the only
 * consumer and provides the GL context. Regression check: headless screenshot (test/shot.sh). */
#ifndef WORLD3D_H
#define WORLD3D_H
#include "constants.h" /* FB_M_PER_DEG_LAT (the ENU-offset <-> lat/lon scale, needed by the ECEF camera) */
#include "protocol.h"
#include <GLES2/gl2.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef W3_USE_OSM
#include "atmo.h"       /* the frame's sun/sky/light, pure — shared by sky, terrain and buildings */
#include "camera.h"     /* attitude -> basis + MVP, pure */
#include "stars.h"      /* catalogue coords + time + place -> direction, pure */
#include "tilesrc_js.h" /* tile bytes from fb-tiles (async cache + osmmesh provider) */
#endif
#ifndef W3_FOV
#define W3_FOV                                                                                     \
  80.0f /* vertical FOV (deg). Wide FPV cam (Caddx Ratel 2 ~164° diag ->                           \
         * ~80° vertical / ~112° horizontal at 16:9). */
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../geo/osmmesh/src/3rdparty/stb_image.h" /* decls only; osmmesh/src/terrain.c has the impl */
#include "gfx/mat4.h"
#include "chunkvtx.h" /* vertex LAYOUT only; the mesh BUILD runs in the worker (chunkmesh_ecef.h) */
#include "gfx/shaders.h"

/* Every GL object the renderer owns, grouped so a program and its uniform/attribute locations live
 * together. Written once in world3d_init, read by the draw passes. */
static struct {
  GLuint pW, pH, pWT, pSky, pStar, vTerr, vBld, hVBO, skyVBO, starVBO;
  int nTerr, nBld;
  GLint stPos, stMag, stBV, stMVP, stDay;
  GLint wPos, wCol, wMVP, wHaze, wLight, hPos, hCol, hScale;
  GLint wtPos, wtUV, wtMVP, wtTex, wtHaze, wtLight, wtNorm, wtSun, wtSunUp;
  GLint skPos, skF, skS, skU, skTan, skAsp, skSun, skMoon, skMoonPh, skCloud, skSunDisc, skDayF;
} w3_gl;

#include "procedural.h"

/* Live OSM streaming: the world is fetched on demand from Shortbread + Copernicus terrain PMTiles
 * around the aircraft's GPS position -- no static geometry, works at any origin. */
#ifdef W3_USE_OSM
#include "osmmesh/mvt.h"
#include "osmmesh/osmmesh.h"
#include "osmmesh/pmtiles.h"
/* Terrain LOD: a chunked quadtree (Ulrich 2002). A node draws ITSELF or its four children, never
 * both -- every square metre is covered by exactly one chunk, so no overlap/offset/z-fighting.
 * Refinement by screen-space error from each chunk's measured geometric error:
 *   SSE = node.error * W3_SSE_K / distance ; split while SSE > W3_EPS. */
#include "terrain/lod.h"

/* Ground: OSM footprints/roads/rivers/rails + landcover baked into one orthographic texture per
 * tile (fb-tiles), draped over the terrain heightfield. */
static osmmesh_ctx *w3_osm = 0; /* terrain heightfield meshes; the "world is open" gate */
/* Origin (home), single owner: cc.c sets lat/lon from /config.js, everything else reads it. lat/lon
 * drive the star alt/az and the tile stream; yoff is the origin ground elevation (AGL + no-telemetry
 * camera-height fallback until /elev answers); tx/ty is the last centre tile. */
typedef struct {
  double lat, lon;
  float yoff;
  int yoff_set;
  int have_tile;
  uint32_t tx, ty;
} w3_origin;
static w3_origin w3_O = {.lat = 52.045,
                         .lon = 9.385}; /* Hameln default until /config.js overrides */
/* Simulated wall-clock override, Unix seconds; 0 = real time. Set from window.FB_SIM_UTC (cc.c) so
 * a night/dusk sky can be pinned reproducibly. The star sidereal time reads it; the aircraft
 * honours the matching SIM_UTC env for the sun, so visibility (sun) and star positions agree on the
 * same instant. */
static double w3_sim_utc = 0;
/* Height above ground, metres, computed in cc.c each frame as telem.alt(ASL) - DEM ground under the
 * aircraft (async /elev, origin-ground fallback). Drives the HUD's AGL readout; the LOD gets it
 * too. */
static float w3_agl = 0;
/* Seed the origin ground before any tile streams, so AGL and the no-telemetry camera height are
 * sane at frame 0 (from /elev, before the centre tile's own probe lands). */
static void w3_seed_yoff(float y) {
  if (!w3_O.yoff_set) {
    w3_O.yoff = y;
    w3_O.yoff_set = 1;
  }
}

/* One draw list, built by the tree walk each stream pass. */
typedef struct {
  GLuint vbo, tex[2];
  int nverts;
  float bmin[3], bmax[3];
  double origin[3]; /* per-tile ECEF origin; the frame subtracts cam_ecef to get the translation */
} w3_tileGL;
static w3_tileGL w3_D[W3_BUDGET];

/* Ground albedo is baked view-independent by fb-tiles (what the ground IS); lighting stays here,
 * per-pixel, from our own sun + DEM normals. */
enum { W3_GROUND_OSM = 0, W3_GROUND_PHOTO = 1 };
/* Albedo source (TAB flips it; index into tex[mode]); dirty stays set while any resident tile lacks
 * the wanted albedo, so world3d_stream doesn't sleep until it's baked. */
static struct {
  int mode;
  int dirty;
} w3_ground = {.mode = W3_GROUND_OSM};
/* Per-pass render/stream bookkeeping, single owner; world3d_stream_at resets the per-pass fields at
 * the top of a pass. cc.c's KEEPALIVE accessors (cc_drawn/cc_visible/cc_mipmaps) read these. */
static struct {
  int nD;   /* draw-list length this pass (indexes w3_D) */
  int nvis; /* of nD, how many last frame's frustum drew; proof the cull bites */
  /* Chunks asked for but not yet resident: the streamer may only sleep at 0 (asked-for-nothing and
   * waiting-for-nothing are different states -- conflating them declared victory over an empty world). */
  int pending;
  /* Drawable (floor 256 present) but still below SSE target -- climbing the ramp. Kept apart from
   * pending: keeps the streamer awake to sharpen without ever blocking "0 pending". */
  int sharpen;
  unsigned pass_mark; /* touch stamp at the start of THIS pass; want_lod_max resets off it, then max's */
  long mipmaps;       /* glGenerateMipmap count -- per-frame thrash counter (~0 warm); read via cc_mipmaps() */
  int split_want, split_wait; /* chunks that wanted to split; couldn't (yet) */
  int over;                   /* splits refused by the budget -> coarser ground */
  int lvl[8];                 /* chunks drawn per level, W3_ROOTZ..W3_MAXZ */
  int cache_hits, cache_bakes, cache_evict;
} w3_frame;

#include "tiles/bake.h"

/* Reader's half of the vertex layout (writer = chunkmesh_ecef.h); a layout change breaks the build. */
#define W3_VTX_STRIDE ((GLsizei)sizeof(w3_vtx))
#define W3_VTX_OFF(f) ((void *)offsetof(w3_vtx, f))

/* Open the tile stream from fb-tiles. Nothing is bundled -- this is what makes any origin work. */
static int world3d_tiles_open(const char *base, double lat, double lon) {
  w3_tiles_init(base);
  osmmesh_config cfg = {.origin_lat = lat,
                        .origin_lon = lon,
                        .tile_provider = w3_tile_provider,
                        .tile_provider_user = 0,
                        .provider_terrain_max_zoom =
                            15, /* Tilezen terrarium; no archive header to read */
                        .enable_terrain = 1,
                        .enable_buildings = 0,
                        .enable_linears = 0};
  if (osmmesh_create(&cfg, &w3_osm) != OSMMESH_OK) {
    printf("[world3d] osmmesh_create (streaming) failed\n");
    w3_osm = 0;
    return 0;
  }
  w3_O.have_tile = 0;
  /* Same origin as the ctx above: the worker's mesh is ENU-relative to it. */
  w3_worker_init(base, lat, lon);
  printf("[world3d] streaming tiles from %s (origin %.4f/%.4f), worker spawned\n", base, lat, lon);
  return 1;
}
#include "tiles/lru.h"

#include "tiles/tilemath.h"
#include "tiles/walk.h"

#include "terrain/stream.h"
#endif /* W3_USE_OSM */

/* Switch ground albedo. Both textures are already resident (each tile carries both), so this is a
 * pure index flip -- no re-bake, no flush, no network. A fallback view must not empty itself first. */
static void w3_ground_toggle(void) {
  w3_ground.mode = (w3_ground.mode == W3_GROUND_OSM) ? W3_GROUND_PHOTO : W3_GROUND_OSM;
  printf("[world3d] ground = %s\n",
         w3_ground.mode == W3_GROUND_PHOTO ? "aerial photo" : "OSM render");
}

#include "max7456.h" /* MAX7456 font atlas + glyph program */
#include "hud.h"     /* HUD/OSD build + render (uses mx_*) */

/* Compile shaders + HUD buffer. Geometry comes from world3d_stream() when tiles are open, else a
 * procedural fallback. */
static void world3d_init(void) {
  /* Trim the texture-LOD ramp to what this context can bind (WebGL2 guarantees >= 2048). */
  {
    GLint mx = 2048;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mx);
    int cap = 0;
    while (cap < W3_NLOD && w3_lod_px[cap] <= (int)mx)
      cap++;
    if (cap > W3_LOD_MAXSTEPS)
      cap = W3_LOD_MAXSTEPS; /* policy ceiling: 2048 is gated (see table) */
    if (cap < 1) cap = 1;
    w3_lod_cap = cap;
    printf("[world3d] LOD ramp: %d steps, top %d px (MAX_TEXTURE_SIZE %d)\n", cap,
           w3_lod_px[cap - 1], (int)mx);
  }
  w3_gl.pW = w3_prog(W3_VSW, W3_FSW);
  w3_gl.wPos = glGetAttribLocation(w3_gl.pW, "aPos");
  w3_gl.wCol = glGetAttribLocation(w3_gl.pW, "aCol");
  w3_gl.wMVP = glGetUniformLocation(w3_gl.pW, "uMVP");
  w3_gl.wHaze = glGetUniformLocation(w3_gl.pW, "uHaze");
  w3_gl.wLight = glGetUniformLocation(w3_gl.pW, "uLight");
  w3_gl.pH = w3_prog(W3_VSH, W3_FSH);
  w3_gl.hPos = glGetAttribLocation(w3_gl.pH, "aPos");
  w3_gl.hCol = glGetAttribLocation(w3_gl.pH, "aCol");
  w3_gl.hScale = glGetUniformLocation(w3_gl.pH, "uScale");
  glGenBuffers(1, &w3_gl.hVBO);
  mx_init(); /* MAX7456 font atlas + glyph program for the HUD text */
  /* sky dome program + a fullscreen quad (two triangles in NDC) */
  w3_gl.pSky = w3_prog(W3_VSKY, W3_FSKY);
  w3_gl.skPos = glGetAttribLocation(w3_gl.pSky, "aPos");
  w3_gl.skF = glGetUniformLocation(w3_gl.pSky, "uF");
  w3_gl.skS = glGetUniformLocation(w3_gl.pSky, "uS");
  w3_gl.skU = glGetUniformLocation(w3_gl.pSky, "uU");
  w3_gl.skTan = glGetUniformLocation(w3_gl.pSky, "uTan");
  w3_gl.skAsp = glGetUniformLocation(w3_gl.pSky, "uAsp");
  w3_gl.skSun = glGetUniformLocation(w3_gl.pSky, "uSun");
  w3_gl.skMoon = glGetUniformLocation(w3_gl.pSky, "uMoon");
  w3_gl.skMoonPh = glGetUniformLocation(w3_gl.pSky, "uMoonPh");
  w3_gl.skCloud = glGetUniformLocation(w3_gl.pSky, "uCloud");
  w3_gl.skSunDisc = glGetUniformLocation(w3_gl.pSky, "uSunDisc");
  w3_gl.skDayF = glGetUniformLocation(w3_gl.pSky, "uDayF");
  {
    float q[12] = {-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1};
    glGenBuffers(1, &w3_gl.skyVBO);
    glBindBuffer(GL_ARRAY_BUFFER, w3_gl.skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STATIC_DRAW);
  }
  w3_gl.pStar = w3_prog(W3_VSTAR, W3_FSTAR);
  w3_gl.stPos = glGetAttribLocation(w3_gl.pStar, "aPos");
  w3_gl.stMag = glGetAttribLocation(w3_gl.pStar, "aMag");
  w3_gl.stBV = glGetAttribLocation(w3_gl.pStar, "aBV");
  w3_gl.stMVP = glGetUniformLocation(w3_gl.pStar, "uMVP");
  w3_gl.stDay = glGetUniformLocation(w3_gl.pStar, "uDay");
  glGenBuffers(1, &w3_gl.starVBO);
#ifdef W3_USE_OSM
  w3_gl.pWT = w3_prog(W3_VSWT, W3_FSWT);
  w3_gl.wtPos = glGetAttribLocation(w3_gl.pWT, "aPos");
  w3_gl.wtUV = glGetAttribLocation(w3_gl.pWT, "aUV");
  w3_gl.wtMVP = glGetUniformLocation(w3_gl.pWT, "uMVP");
  w3_gl.wtTex = glGetUniformLocation(w3_gl.pWT, "uTex");
  w3_gl.wtHaze = glGetUniformLocation(w3_gl.pWT, "uHaze");
  w3_gl.wtLight = glGetUniformLocation(w3_gl.pWT, "uLight");
  w3_gl.wtNorm = glGetAttribLocation(w3_gl.pWT, "aNorm");
  w3_gl.wtSun = glGetUniformLocation(w3_gl.pWT, "uSun");
  w3_gl.wtSunUp = glGetUniformLocation(w3_gl.pWT, "uSunUp");
  if (w3_osm) return; /* geometry (textured tiles) comes from world3d_stream() */
#endif
  w3_build_procedural();
}

#include "sky.h"    /* sky + star passes */
#include "render.h" /* terrain/fallback passes + world3d_render_scene */
#endif
