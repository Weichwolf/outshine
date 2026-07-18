/* FlightBox — shared 3D world + HUD renderer (GLES2/WebGL).
 * Included by cc.c, the WASM command center — the only consumer. There used to be a second,
 * native renderer (render_native.c) that included this too "so both draw the same thing"; the
 * two drifted anyway (it compiled W3_TERR=24/W3_FARTEX=256 while the browser shipped 22/512).
 * The regression check is now a headless browser screenshot of the real app (test/shot.sh), so
 * there is one renderer and nothing to keep in sync. Caller provides the GL context. */
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

/* mat4 / vec3 maths: GL-free, so it lives in gfx/mat4.h and is unit-tested directly. */
/* stb_image: declarations only -- osmmesh/src/terrain.c owns the one implementation we link. */
#include "../geo/osmmesh/src/3rdparty/stb_image.h"
#include "gfx/mat4.h"
/* The main thread needs only the vertex LAYOUT (to set glVertexAttribPointer and size the VBO) --
 * the mesh BUILD (w3_chunk_build_ecef) runs in the tile worker now, so including chunkmesh_ecef.h
 * here would be a -Wunused-function error. The worker includes it; both share chunkvtx.h. */
#include "chunkvtx.h"

/* ---- GL program helpers ---- */
#include "gfx/shaders.h"

/* Every GL object the renderer owns: one program handle + its uniform/attribute locations per
 * shader, plus the VBOs and the fallback vertex counts. Written once in world3d_init, read by the
 * draw passes. Grouped so a program and the locations that index into it live together instead of
 * as ~40 loose globals. */
static struct {
  GLuint pW, pH, pWT, pSky, pStar, vTerr, vBld, hVBO, skyVBO, starVBO;
  int nTerr, nBld;
  GLint stPos, stMag, stBV, stMVP, stDay;
  GLint wPos, wCol, wMVP, wHaze, wLight, hPos, hCol, hScale;
  GLint wtPos, wtUV, wtMVP, wtTex, wtHaze, wtLight, wtNorm, wtSun, wtSunUp;
  GLint skPos, skF, skS, skU, skTan, skAsp, skSun, skMoon, skMoonPh, skCloud, skSunDisc, skDayF;
} w3_gl;

#include "procedural.h"

/* ---- live OSM streaming (osmmesh) -------------------------------------------
 * Location-agnostic: the world is streamed on demand from OSM Shortbread +
 * Copernicus terrain PMTiles around the aircraft's current GPS position. No
 * static geometry — as the aircraft crosses a tile boundary the grid is re-fetched
 * and the terrain/building VBOs are rebuilt. Origin (lat/lon) is configurable. */
#ifdef W3_USE_OSM
#include "osmmesh/mvt.h"
#include "osmmesh/osmmesh.h"
#include "osmmesh/pmtiles.h"
/* ---- terrain LOD: a chunked quadtree (Ulrich 2002), not a stack of rings -------------------
 *
 * What was here before: three fixed 5x5 grids at z14/z11/z8, drawn coarse-to-fine with a polygon
 * offset so the fine one "wins". They overlapped COMPLETELY -- the ground under the aircraft was
 * rasterised three times -- and that was not an oversight to be tuned away, it was unfixable by
 * construction:
 *
 *   - 5x5 z14 spans 7.5 km; ONE z11 tile spans 12.0 km. No z11 tile is ever fully covered, so not
 *     one of them could be skipped. Same for z11 vs z8. 0 of the 50 coarse tiles were droppable.
 *   - MEASURED at the origin: the z8 surface sits ABOVE the z14 surface at 72.8 % of near-field
 *     points, by up to +85 m; z11 by up to +30 m at 55.8 %. The coarse mesh does not merely
 *     disagree, it pokes through.
 *   - glPolygonOffset cannot fix that. It biases DEPTH, not position, and the bias it buys is a
 *     function of the triangle's depth slope: ~9 m of world depth for z11 at 1 km, ~37 m at 3 km,
 *     ~584 m at 30 km. Against a 30 m intrusion that is a coin flip that lands differently per
 *     triangle and per viewing angle -- which is exactly what "es ueberschneidet sich an den
 *     raendern" looks like. Too weak up close, absurdly strong far away.
 *
 * A quadtree has a HOLE where the finer level sits: a node draws ITSELF or its four children,
 * never both. Every square metre of ground is covered by exactly one chunk. No overlap, no
 * offset, no fighting -- the problem is removed rather than biased.
 *
 * Refinement is by screen-space error, from each chunk's OWN measured geometric error:
 *     SSE = node.error * W3_SSE_K / distance      (pixels)
 *     split while SSE > W3_EPS
 * See chunkmesh_ecef.h (w3_chunk_build_ecef) for why the error must be measured per chunk, not
 * tabulated per zoom. */
#include "terrain/lod.h"

/* Old-flight-sim ground: OSM footprints/roads/rivers/rails + landcover are baked
 * into ONE orthographic texture per tile (no building geometry). The texture is
 * draped over the terrain heightfield. The vector features come from the Shortbread
 * PMTiles via osmmesh's MVT decoder; the terrain heightfield from osmmesh's terrain. */
static osmmesh_ctx *w3_osm = 0; /* terrain heightfield meshes; the "world is open" gate */
/* The origin (home) and everything anchored to it. ONE owner: cc.c sets lat/lon from /config.js
 * once, everything else reads it -- the old g_olat/g_olon (cc.c) and w3_olat/w3_olon (here) were
 * the same fact stored twice. lat/lon drive the star alt/az and the tile stream; yoff is the origin
 * ground elevation, the AGL and no-telemetry-camera-height fallback until /elev answers (NOT a
 * camera lift any more -- ECEF vertices are absolute on the ellipsoid); tx/ty is the last centre
 * tile. */
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

/* ONE draw list, built by the tree walk each stream pass. There used to be three of these
 * (w3_T/w3_TF/w3_TF2 with w3_nT/w3_nTF/w3_nTF2) -- the same code three times with suffixes,
 * which is how the overlap got written down as a fact and then lived on as one. */
typedef struct {
  GLuint vbo, tex[2];
  int nverts;
  float bmin[3], bmax[3];
  double origin[3]; /* per-tile ECEF origin; the frame subtracts cam_ecef to get the translation */
} w3_tileGL;
static w3_tileGL w3_D[W3_BUDGET];

/* ---- ground albedo: downloaded ready-made from fb-tiles ------------------------------------
 * There used to be ~150 lines of software rasteriser here: scanline polygon fill, thick lines,
 * MVT layer walking, the aerial mosaic blit. It ran in the browser on EVERY page load, re-drawing
 * cartography that had not changed since the last one, from a vector tile plus its 4 seam
 * neighbours. Now fb-tiles does it once and keeps the result (tiles/raster.c, tiles/bake.c), so
 * this is a download and an upload.
 *
 * The albedo is view-independent -- what the ground IS, not how it is lit -- which is exactly why
 * it may be precomputed at all. Lighting stays here, per-pixel, from our own sun and the DEM
 * normals. Baking an albedo is not baking light.
 */
enum { W3_GROUND_OSM = 0, W3_GROUND_PHOTO = 1 };
/* The ground albedo source (TAB flips it; an index into tex[mode]) and the "not done yet" flag:
 * dirty is set while any resident tile still lacks the albedo we want, keeping world3d_stream from
 * declaring victory and sleeping until the next tile boundary. */
static struct {
  int mode;
  int dirty;
} w3_ground = {.mode = W3_GROUND_OSM};
/* Per-pass render/stream bookkeeping: counters reset or accumulated each stream pass, plus the two
 * running thrash/diagnostic totals the harness reads. One owner; world3d_stream_at resets the
 * per-pass fields at the top of a pass. The KEEPALIVE accessors in cc.c
 * (cc_drawn/cc_visible/cc_mipmaps) expose fields -- their names are unchanged, only their bodies
 * now read w3_frame. */
static struct {
  int nD;   /* draw-list length this pass (indexes w3_D) */
  int nvis; /* of nD, how many last frame's frustum drew; proof the cull bites */
  /* Chunks the tree asked for and did not get. The streamer may only sleep when this is 0:
   * "nothing is waiting" and "nothing was asked for" are different states, and conflating them let
   * the tree declare victory over an EMPTY world and never look again. */
  int pending;
  /* Chunks that ARE drawable (floor 256 present) but still below their SSE target -- climbing the
   * ramp. Kept apart from pending on purpose: "shown" and "fully sharp" are different states, only
   * the FIRST gates convergence. Keeps the streamer awake to climb, never blocks "0 pending" --
   * else a moving camera, always mid-climb on some near tile (a fresh 2048 is 7-20 s), would never
   * latch. */
  int sharpen;
  unsigned
      pass_mark; /* touch stamp at the start of THIS pass; want_lod_max resets off it, then max's */
  /* Every glGenerateMipmap the tile path does -- a clean per-frame thrash counter (~0 warm,
   * explodes if a tile re-bakes each frame). The whole proof of the side-by-side LOD slots; read
   * via cc_mipmaps(). */
  long mipmaps;
  int split_want, split_wait; /* chunks that wanted to split; couldn't (yet) */
  int over;                   /* splits refused by the budget -> coarser ground */
  int lvl[8];                 /* chunks drawn per level, W3_ROOTZ..W3_MAXZ */
  int cache_hits, cache_bakes, cache_evict;
} w3_frame;

#include "tiles/bake.h"

/* The terrain vertex layout is defined by its WRITER, in chunkmesh_ecef.h. These two macros are the
 * READER's half of that contract -- they need GL types, so they stay here, but they derive from
 * the same struct, so a layout change breaks the build rather than the picture. */
#define W3_VTX_STRIDE ((GLsizei)sizeof(w3_vtx))
#define W3_VTX_OFF(f) ((void *)offsetof(w3_vtx, f))

/* w3_chunk_build_ecef (chunkmesh_ecef.h) -- decimation, smooth normals, the measured error, the
 * skirt -- now runs in the WORKER (tileworker.c), which hands back a finished w3_vtx[] array. All
 * that is left on the main thread is glBufferData, inline in w3_cache_get's MESH->READY transition.
 * The GL upload wrapper that used to live here (w3_terr_vbo) is gone with the synchronous path. */

/* Stream tiles on demand from the fb-tiles service. Nothing is bundled, everything is fetched --
 * this is what makes any origin on earth work. */
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
  /* Spawn the tile worker with the SAME origin: the mesh it builds is ENU-relative to it. The main
   * thread's osmmesh ctx above is now only the "world is open" gate and osmmesh_geo_to_tile -- the
   * fetch/decode/mesh it used to do runs in the worker. */
  w3_worker_init(base, lat, lon);
  printf("[world3d] streaming tiles from %s (origin %.4f/%.4f), worker spawned\n", base, lat, lon);
  return 1;
}
#include "tiles/lru.h"

#include "tiles/tilemath.h"
#include "tiles/walk.h"

#include "terrain/stream.h"
#endif /* W3_USE_OSM */

/* Switch the ground albedo source. Deliberately throws NOTHING away: each cached tile carries the
 * mode it was baked from, and w3_cache_get re-bakes it in place once the new albedo is on hand.
 * The geometry (VBO) is identical either way, so only the texture is redone.
 *
 * The first version deleted every baked tile here. That emptied the world until everything had
 * re-baked -- which reads as "the switch takes forever" even though the tiles were already on the
 * server's disk in under a millisecond. A fallback view you reach for when the camera dies must
 * not begin by removing the view. */
static void w3_ground_toggle(void) {
  /* Both textures are already resident, so this really is just an index flip -- no re-bake, no
   * flush, no network. Earlier versions re-baked here and the switch took about a minute, because
   * world3d_stream returns early once everything is loaded and only wakes when the aircraft
   * crosses a tile boundary (~1 min in a 1000 m orbit). The pictures were on disk the whole time.
   */
  w3_ground.mode = (w3_ground.mode == W3_GROUND_OSM) ? W3_GROUND_PHOTO : W3_GROUND_OSM;
  printf("[world3d] ground = %s\n",
         w3_ground.mode == W3_GROUND_PHOTO ? "aerial photo" : "OSM render");
}

#include "max7456.h" /* MAX7456 font atlas + glyph program */
#include "hud.h"     /* HUD/OSD build + render (uses mx_*) */

/* ---- init + render ----
 * Compiles shaders + HUD buffer. Geometry is provided by world3d_stream()
 * (live osmmesh) when an archive has been opened; otherwise a procedural world
 * is built as a standalone fallback. */
static void world3d_init(void) {
  /* Trim the texture-LOD ramp to what this context can actually hold. WebGL2 guarantees >= 2048,
   * so normally all four steps survive; a spec-minimum context simply never asks for a size it
   * cannot bind, rather than failing the glTexImage2D silently at draw time. */
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
