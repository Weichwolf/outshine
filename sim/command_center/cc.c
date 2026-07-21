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
#include "net.h"       /* telemetry/control WebSocket; owns `telem`/`have_telem` */
#include "present.h"   /* render targets + present pipeline (SVS/EVS/HUD, dynamic display res) */

#define WIN_W 1280 /* initial canvas size; present.h re-syncs to the display each frame */
#define WIN_H 720

static SDL_Window *win;
static SDL_GameController *pad = NULL;

EMSCRIPTEN_KEEPALIVE void cc_toggle_ground(void) {
  w3_ground_toggle();
}
/* Harness counters (per-frame deltas): mipmaps = tile-cache thrash (~0 warm), texvram vs the 2 GB
 * cap, visible<drawn = the frustum cull works. */
EMSCRIPTEN_KEEPALIVE long cc_mipmaps(void) {
  return w3_frame.mipmaps;
}
EMSCRIPTEN_KEEPALIVE long cc_texvram(void) {
  return w3_texvram();
}
EMSCRIPTEN_KEEPALIVE int cc_drawn(void) {
  return w3_frame.nD;
}
EMSCRIPTEN_KEEPALIVE int cc_visible(void) {
  return w3_frame.nvis;
}

static int armed_latch = 0, prev_enter = 0;
static uint16_t seq = 0;
static void send_control(void) {
  if (!ws_open) return;
  ctrl_packet_t c;
  memset(&c, 0, sizeof c);
  c.magic = FB_MAGIC_CTRL;
  c.link_up = 1;
  c.throttle = 0.5f;
  const Uint8 *k = SDL_GetKeyboardState(0);
  if (k[SDL_SCANCODE_RIGHT]) c.roll += 1;
  if (k[SDL_SCANCODE_LEFT]) c.roll -= 1;
  if (k[SDL_SCANCODE_UP]) c.pitch += 1;
  if (k[SDL_SCANCODE_DOWN]) c.pitch -= 1;
  if (k[SDL_SCANCODE_D]) c.yaw += 1;
  if (k[SDL_SCANCODE_A]) c.yaw -= 1;
  if (k[SDL_SCANCODE_W]) c.throttle = 1;
  if (k[SDL_SCANCODE_S]) c.throttle = 0;
  if (k[SDL_SCANCODE_L]) c.link_up = 0;
  int en = k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_SPACE];
  if (en && !prev_enter) armed_latch = 1;
  prev_enter = en;
  if (pad) {
    float rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.f,
          ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f;
    float lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f,
          ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
    c.roll += rx;
    c.pitch += ry;
    c.yaw += lx;
    if (ly < -0.1f) c.throttle = -ly;
    (void)lx;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) armed_latch = 1;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) c.link_up = 0;
  }
  c.arm = armed_latch;
  c.seq = seq++;
  emscripten_websocket_send_binary(ws, &c, sizeof c);
}

static void frame(void) {
  SDL_Event e;
  while (SDL_PollEvent(&e))
    if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
  send_control();
  /* Sample the LATEST pose each frame (telemetry 100 Hz > 60 fps render): smooth, zero added
   * latency. */
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
    double ground = fb_ground_get();
    if (ground <= -1e8) ground = w3_O.yoff;
    double agl = (double)telem.alt - ground;
    w3_agl = (float)agl;                                       /* TRUE AGL — negative = below terrain, never hide it */
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
  const char *gc = emscripten_run_script_string(
      "(typeof window.FB_GROUND_CLEAR==='number'?window.FB_GROUND_CLEAR:0).toString()");
  w3_cam_clear = (float)atof(gc);
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
  net_init();
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
