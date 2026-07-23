/* FlightBox Command Center (C -> WASM): the ground station. Samples the telemetry pose, drives the
 * osmmesh world (world3d.h) and the present pipeline (present.h), sends control back. Thin main +
 * input loop; the render targets, video link, fetches and socket live in their own headers.
 * Keys: arrows roll/pitch, A/D yaw, W/S throttle, ENTER arm, L drop link (TAB ground, F fullscreen
 * are handled in index.html -- the browser eats TAB for focus before SDL sees it). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/websocket.h>
#define W3_USE_OSM
#include "world3d.h"

#include "codec.h"     /* EVS video link: WebCodecs H.264 (fb_codec_*) */
#include "constants.h" /* GL enums the ES2 header lacks + MSAA entry points, FB_M_PER_DEG_LAT */
#include "jsbridge.h"  /* fb-tiles fetches: /elev (origin + ground), /stars */
#include "flightsim.h" /* LOCAL JSBSim + FBW/loiter; owns `telem`/`have_telem` (replaces net.h) */
#include "present.h"   /* render targets + present pipeline (SVS/EVS/HUD, dynamic display res) */

#define WIN_W 1280 /* initial canvas size; present.h re-syncs to the display each frame */
#define WIN_H 720

static SDL_Window *win;
static SDL_GameController *pad = NULL;

extern "C" EMSCRIPTEN_KEEPALIVE void cc_toggle_ground(void) {
  w3_ground_toggle();
}
/* Harness counters (per-frame deltas): mipmaps = tile-cache thrash (~0 warm), texvram vs
 * w3_vram_budget (default 1.5 GiB, FB_VRAM_MB), visible<drawn = the frustum cull works. */
extern "C" EMSCRIPTEN_KEEPALIVE long cc_mipmaps(void) {
  return w3_frame.mipmaps;
}
extern "C" EMSCRIPTEN_KEEPALIVE long cc_texvram(void) {
  return w3_texvram();
}
extern "C" EMSCRIPTEN_KEEPALIVE int cc_drawn(void) {
  return w3_frame.nD;
}
extern "C" EMSCRIPTEN_KEEPALIVE int cc_visible(void) {
  return w3_frame.nvis;
}

static void frame(void) {
  SDL_Event e;
  while (SDL_PollEvent(&e))
    if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
  flightsim_step(1.0 / 60.0);   /* advance the in-process F-16 (JSBSim + FBW/loiter), publish -> telem */
  /* Sample the LATEST pose each frame (physics 100 Hz > 60 fps render): smooth, zero added latency. */
  if (have_telem) {
    double lat = w3_O.lat + (double)telem.y / FB_M_PER_DEG_LAT;
    double lon = w3_O.lon + (double)telem.x / (FB_M_PER_DEG_LAT * cos(w3_O.lat * M_PI / 180.0));
    /* AGL is the LOD/HUD distance term. Ground under the aircraft is an ASYNC /elev (off the frame
     * loop, re-requested per ~30 m); fall back to origin ground until the first sample lands.
     * Feeding ASL would refine the ground for the wrong distance. */
    static double g_glat = 1e9, g_glon = 1e9;
    if (fabs(lat - g_glat) > 3.0e-4 || fabs(lon - g_glon) > 4.5e-4) {
      fb_ground_request(lat, lon);
      g_glat = lat;
      g_glon = lon;
    }
    double ground_raw = fb_ground_get();
    double ground = ground_raw > -1e8 ? ground_raw : w3_O.yoff;
    double agl = (double)telem.alt - ground;
    w3_agl = (float)agl;
    /* JSBSim's collision floor = the SAME DEM the HUD/renderer use (CLAUDE.md crash contract): a
     * gear-down touchdown or a crash must happen against the real terrain under the aircraft, not
     * ASL 0 (the Jura here is ~1000-1300 m -- ground reactions would be blind without this). Only
     * push REAL /elev samples (ground_raw, not the w3_O.yoff fallback) -- a cold/unknown sample
     * leaves JSBSim at its last good value (set once at spawn in flightsim_init) instead of
     * snapping to a possibly-stale default. One frame of lag (physics 100 Hz, this runs at 60 fps)
     * is fine -- xpjsb (the retired IPC bridge) only ever updated it periodically too. */
    if (ground_raw > -1e8) fb_jsbsim_set_ground(ground_raw);
    { /* AGL/ground invariant log @1 Hz: ground = alt - agl must stay plausible (sim-critic run 3) */
      static int inv_n = 0;
      if (++inv_n >= 60) { inv_n = 0; printf("[agl] alt=%.0f agl=%.0f ground=%.0f fdmGnd=%.0f\n", (double)telem.alt, agl, ground, fb_jsbsim_get_ground()); }
    }                                       /* TRUE AGL — negative = below terrain, never hide it */
    world3d_stream_at(lat, lon, agl < 1.0 ? 1.0 : agl);       /* streaming LOD still needs a positive height */
  }
  fb_present_frame(&telem, have_telem);
}

/* Runtime config from window (/config.js): origin -> w3_O, tiles URL -> caller. `typeof` (not
 * `!=0`) keeps a real 0 origin (equator/prime meridian) from snapping back to Hameln.
 * run_script_string hands back a REUSED buffer, so each result is consumed before the next call. */
static void cfg_from_js(char *tiles_url, size_t n) {
  const char *sl = emscripten_run_script_string(
      "(typeof window.FB_ORIGIN_LAT==='number'?window.FB_ORIGIN_LAT:'').toString()");
  if (sl && *sl) w3_O.lat = atof(sl);
  const char *so = emscripten_run_script_string(
      "(typeof window.FB_ORIGIN_LON==='number'?window.FB_ORIGIN_LON:'').toString()");
  if (so && *so) w3_O.lon = atof(so);
  /* Simulated-UTC override (Unix seconds; 0/unset = real time): pins a reproducible night/dusk sky.
   */
  const char *su = emscripten_run_script_string(
      "(typeof window.FB_SIM_UTC==='number'?window.FB_SIM_UTC:0).toString()");
  w3_sim_utc = atof(su);
  /* Test OVERRIDE only: the normal case is flightsim_init filling w3_cam_clear from the loaded
   * model's own geometry (fb_jsbsim_ground_clearance, in-process) -- the xpjsb-bridge IPC channel
   * this used to read from is retired. 0/unset leaves that geometry-true default alone. */
  const char *gc = emscripten_run_script_string(
      "(typeof window.FB_GROUND_CLEAR==='number'?window.FB_GROUND_CLEAR:0).toString()");
  if (atof(gc) > 0) w3_cam_clear = (float)atof(gc);
  /* View distance is a SETTING (km); tile/texture buffers scale with it on demand. */
  const char *vk = emscripten_run_script_string(
      "(typeof window.FB_VIEW_KM==='number'?window.FB_VIEW_KM:0).toString()");
  if (atof(vk) > 0) w3_view_m = (float)(atof(vk) * 1000.0);
  /* Tile-cache eviction budget (MB); default stays 1.5 GiB (w3_vram_budget's compiled-in value). */
  const char *vm = emscripten_run_script_string(
      "(typeof window.FB_VRAM_MB==='number'?window.FB_VRAM_MB:0).toString()");
  if (atof(vm) > 0) w3_vram_budget = (long)(atof(vm) * 1024.0 * 1024.0);
  snprintf(tiles_url, n, "%s",
           emscripten_run_script_string("(window.FB_TILES_URL||'').toString()"));
}

/* 4 HYG magnitude bands (~53 KB, static) into one buffer -- concatenated they stay mag-sorted --
 * decoded into w3_stars. A failed fetch leaves a blank night sky, never blocks startup. */
static void stars_load_from_tiles(void) {
  const int cap = 96 * 1024;
  uint8_t *buf = (uint8_t *)malloc((size_t)cap);
  if (!buf) return;
  int off = 0, ok = 1;
  for (int b = 0; b < 4; b++) {
    int n = fb_fetch_stars(b, buf + off, cap - off);
    if (n < 0) {
      printf("[cc] star band %d fetch failed\n", b);
      ok = 0;
      break;
    }
    off += n;
  }
  if (ok) {
    int ns = w3_stars_load(buf, off);
    printf("[cc] star catalogue: %d stars, %d bytes\n", ns, off);
  }
  free(buf);
}

int main(void) {
  char tiles_url[160];
  cfg_from_js(tiles_url, sizeof tiles_url);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); /* WebGL2 — VideoFrame texture upload */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  win = SDL_CreateWindow("FlightBox", 0, 0, WIN_W, WIN_H, SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(win);
  emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
                                    "EXT_texture_filter_anisotropic");

  {
    double he = fb_fetch_elev(
        w3_O.lat, w3_O.lon); /* seed camera lift before streaming; else spawn is underground */
    if (he > -1e8) {
      w3_seed_yoff((float)he);
      printf("[cc] origin ground %.1f m (/elev), camera seeded\n", he);
    } else
      printf("[cc] /elev unreachable at startup — lift waits for the origin tile\n");
  }
  /* Stream every tile on demand from fb-tiles — works at ANY origin on earth. */
  if (tiles_url[0] && world3d_tiles_open(tiles_url, w3_O.lat, w3_O.lon))
    printf("[cc] tiles: streaming from %s, origin %.4f/%.4f\n", tiles_url, w3_O.lat, w3_O.lon);
  else
    printf("[cc] no tiles url or open failed — procedural fallback\n");
  world3d_init();
  stars_load_from_tiles();

  fb_present_init(win);
  {
    double gnd = fb_fetch_elev(w3_O.lat, w3_O.lon);
    flightsim_init(w3_O.lat, w3_O.lon, (gnd > -1e8 ? gnd : 0.0) + 1500.0, 8000.0, gnd);
    printf("[cc] F-16 loitering at %.4f/%.4f, alt %.0f m, R 8 km (JSBSim in-process)\n",
           w3_O.lat, w3_O.lon, (gnd > -1e8 ? gnd : 0.0) + 1500.0);
  }
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
