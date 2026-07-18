/* FlightBox — Command Center (C -> WASM), 3D world camera (Teil B).
 * WebGL/GLES2, all in C.
 *
 * The "camera image" models the real FPV chain: the aircraft's camera (our live
 * osmmesh terrain, rendered from the telemetry pose) is rendered to a low-res FBO,
 * then run through the browser's H.264 VideoEncoder->VideoDecoder (WebCodecs) at a
 * low bitrate — the fixed-function video hardware. That produces REAL compression
 * artifacts (blocking, mosquito noise), standing in for the lossy analog 5.8 GHz
 * downlink. The decoded frame is upscaled onto the canvas and the F-16 HUD is drawn
 * CRISP on top — exactly as a ground station overlays telemetry on received video.
 * (Technique adapted from ~/Git/wasm-dvd-gl.) Without WebCodecs (e.g. Firefox) the
 * raw FBO is shown directly — soft upscale, no artifacts.
 *
 * All 3D + HUD live in world3d.h, of which this file is the ONLY consumer. That used to be
 * "so this browser view matches the native renderer" — there is no native renderer since
 * f36f147; the visual check is a headless browser on this very artifact (test/shot.sh).
 * Keys: arrows roll/pitch, A/D yaw, W/S throttle, ENTER arm, L drop link,
 * TAB ground = OSM render <-> aerial photo (F = fullscreen, both handled in index.html). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/html5.h>
#define W3_USE_OSM
#include "world3d.h"

#include "constants.h"   /* invariants: GL enums the ES2 header lacks + the MSAA entry points, FB_M_PER_DEG_LAT */
#include "codec.h"       /* EVS video link: WebCodecs H.264 encode->decode (fb_codec_*) */
#include "present.h"     /* render targets + present pipeline (SVS/EVS/HUD, dynamic display res) */

#define WIN_W 1280          /* initial canvas size; present.h re-syncs to the display each frame */
#define WIN_H 720

/* Sync XHR, startup only: one request before the render loop so w3_O.yoff is known at frame 0. */
EM_JS(double, fb_fetch_elev, (double lat, double lon), {
  try {
    var base = window.FB_TILES_URL; if(!base) return -1e9;
    var x = new XMLHttpRequest();
    /* block=1: wait for the origin DEM tile so yoff is the REAL ground on the first (only) call.
     * Without it a cold origin returns 503 and the camera spawns underground until the worker's
     * yoff lands many seconds later. Startup-only + sync, so the brief wait is off the frame loop. */
    x.open('GET', base + '/elev?lat=' + lat + '&lon=' + lon + '&block=1', false);
    x.send(null);
    if(x.status>=200 && x.status<300){ var v=parseFloat(x.responseText); if(isFinite(v)) return v; }
  } catch(e){}
  return -1e9;
})
/* Ground elevation under the aircraft, fetched ASYNC so it never blocks the frame loop (unlike the
 * startup sync fb_fetch_elev): fb_ground_request kicks off a /elev fetch if none is in flight,
 * fb_ground_get returns the last resolved ASL ground (-1e9 until the first lands). The C side
 * throttles it by only requesting after ~30 m of travel. */
EM_JS(void, fb_ground_request, (double lat, double lon), {
  var G = Module.__fbGround || (Module.__fbGround = { val:-1e9, busy:false });
  if(G.busy) return; var base = window.FB_TILES_URL; if(!base) return; G.busy=true;
  fetch(base + '/elev?lat=' + lat + '&lon=' + lon)
    .then(function(r){ return r.ok ? r.text() : null; })
    .then(function(t){ if(t!==null){ var v=parseFloat(t); if(isFinite(v)) Module.__fbGround.val=v; } Module.__fbGround.busy=false; })
    .catch(function(){ Module.__fbGround.busy=false; });
})
EM_JS(double, fb_ground_get, (void), { var G=Module.__fbGround; return G?G.val:-1e9; })
/* Fetch one HYG star band synchronously into the WASM heap. Binary over a SYNC XHR needs the
 * x-user-defined charset trick (a sync XHR may not set responseType='arraybuffer'): each character
 * of responseText is then exactly one raw byte. Startup only, like fb_fetch_elev. Returns bytes or -1. */
EM_JS(int, fb_fetch_stars, (int band, uint8_t *dst, int maxbytes), {
  try {
    var base = window.FB_TILES_URL; if(!base) return -1;
    var x = new XMLHttpRequest();
    x.open('GET', base + '/t/stars/' + band + '/0/0', false);
    x.overrideMimeType('text/plain; charset=x-user-defined');
    x.send(null);
    if(x.status>=200 && x.status<300){
      var s = x.responseText, n = s.length;
      if(n > maxbytes) return -1;
      for(var i=0;i<n;i++) HEAPU8[dst+i] = s.charCodeAt(i) & 0xff;
      return n;
    }
  } catch(e){}
  return -1;
})
static SDL_Window *win;
static EMSCRIPTEN_WEBSOCKET_T ws; static int ws_open=0;
static telem_packet_t telem; static int have_telem=0;
static SDL_GameController *pad=NULL;

/* ---------------- websocket ---------------- */
static EM_BOOL on_open(int t,const EmscriptenWebSocketOpenEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=1;return 1;}
static EM_BOOL on_close(int t,const EmscriptenWebSocketCloseEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=0;return 1;}
static EM_BOOL on_msg(int t,const EmscriptenWebSocketMessageEvent*e,void*u){(void)t;(void)u;
  if(e->numBytes<4) return 1; uint32_t mg; memcpy(&mg,e->data,4);
  if(mg==FB_MAGIC_TELEM && e->numBytes==sizeof(telem_packet_t)){ memcpy(&telem,e->data,sizeof telem); have_telem=1; }
  return 1;
}
/* TAB is handled in index.html, not by SDL: the browser uses Tab for focus traversal, so it has
 * to be preventDefault()ed at the DOM before SDL ever sees it. */
EMSCRIPTEN_KEEPALIVE void cc_toggle_ground(void){ w3_ground_toggle(); }
/* glGenerateMipmap count over the whole run -- the tile-cache thrash counter. Sampled per frame by
 * the LOD proof harness (per-frame delta -> p50); ~0 warm, explodes if a tile re-bakes each frame. */
EMSCRIPTEN_KEEPALIVE long cc_mipmaps(void){ return w3_frame.mipmaps; }
/* Resident texture VRAM in bytes -- real, summed from the cache (world3d.h), for the ramp's VRAM
 * budget check. Sampled by the harness against the 2 GB cap. */
EMSCRIPTEN_KEEPALIVE long cc_texvram(void){ return w3_texvram(); }
/* Walk's draw list vs what the frustum actually drew: cc_visible < cc_drawn = the cull works. */
EMSCRIPTEN_KEEPALIVE int cc_drawn(void){ return w3_frame.nD; }
EMSCRIPTEN_KEEPALIVE int cc_visible(void){ return w3_frame.nvis; }

static int armed_latch=0, prev_enter=0; static uint16_t seq=0;
static void send_control(void){
  if(!ws_open)return; ctrl_packet_t c; memset(&c,0,sizeof c); c.magic=FB_MAGIC_CTRL; c.link_up=1; c.throttle=0.5f;
  const Uint8*k=SDL_GetKeyboardState(0);
  if(k[SDL_SCANCODE_RIGHT])c.roll+=1; if(k[SDL_SCANCODE_LEFT])c.roll-=1;
  if(k[SDL_SCANCODE_UP])c.pitch+=1; if(k[SDL_SCANCODE_DOWN])c.pitch-=1;
  if(k[SDL_SCANCODE_D])c.yaw+=1; if(k[SDL_SCANCODE_A])c.yaw-=1;
  if(k[SDL_SCANCODE_W])c.throttle=1; if(k[SDL_SCANCODE_S])c.throttle=0;
  if(k[SDL_SCANCODE_L])c.link_up=0;
  int en=k[SDL_SCANCODE_RETURN]||k[SDL_SCANCODE_SPACE]; if(en&&!prev_enter)armed_latch=1; prev_enter=en;
  if(pad){ float rx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX)/32767.f,ry=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY)/32767.f;
    float lx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX)/32767.f,ly=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY)/32767.f;
    c.roll+=rx; c.pitch+=ry; c.yaw+=lx; if(ly<-0.1f)c.throttle=-ly; (void)lx;
    if(SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A))armed_latch=1;
    if(SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_B))c.link_up=0; }
  c.arm=armed_latch; c.seq=seq++; emscripten_websocket_send_binary(ws,&c,sizeof c);
}

static void frame(void){
  SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_CONTROLLERDEVICEADDED&&!pad) pad=SDL_GameControllerOpen(e.cdevice.which);
  send_control();
  /* Game-engine style: telemetry (the state) arrives faster than we render (100 Hz vs
   * 60 fps), so we just sample the LATEST pose each frame — smooth motion, zero added
   * latency (no interpolation delay). */
  if(have_telem){
    double lat = w3_O.lat + (double)telem.y/FB_M_PER_DEG_LAT;
    double lon = w3_O.lon + (double)telem.x/(FB_M_PER_DEG_LAT*cos(w3_O.lat*M_PI/180.0));
    /* telem.alt is ASL now; AGL (height above ground) drives the LOD and the HUD. Ground under the
     * aircraft comes from an ASYNC /elev (never blocks the frame loop), re-requested only after ~30 m
     * of travel; until the first sample lands we fall back to the origin ground so AGL is sane at
     * frame 0. Height above ground is the distance term for every chunk under the aircraft -- feeding
     * ASL would refine the ground for the wrong distance. */
    static double g_glat=1e9, g_glon=1e9;
    if(fabs(lat-g_glat)>3.0e-4 || fabs(lon-g_glon)>4.5e-4){ fb_ground_request(lat,lon); g_glat=lat; g_glon=lon; }
    double ground = fb_ground_get(); if(ground<=-1e8) ground = w3_O.yoff;
    double agl = (double)telem.alt - ground; if(agl < 1.0) agl = 1.0;
    w3_agl = (float)agl;
    world3d_stream_at(lat, lon, agl);
  }
  fb_present_frame(&telem, have_telem);
}

/* All runtime config /config.js puts on window: the origin (home) coords into w3_O, the fb-tiles
 * base URL into the caller's buffer.
 *
 * The server always emits FB_ORIGIN_LAT/LON as a NUMBER (the Hameln default when the env is unset),
 * so 0 is a REAL origin -- the equator / prime meridian -- not "missing". typeof separates a
 * genuinely absent value (config.js never loaded) from 0; an earlier `!=0` snapped an equatorial
 * origin back to Hameln. emscripten_run_script_string returns a REUSED buffer, so each result is
 * consumed before the next call (holding two once put the whole world at 9.385,9.385). */
static void cfg_from_js(char *tiles_url, size_t n){
  const char*sl=emscripten_run_script_string("(typeof window.FB_ORIGIN_LAT==='number'?window.FB_ORIGIN_LAT:'').toString()");
  if(sl&&*sl) w3_O.lat=atof(sl);
  const char*so=emscripten_run_script_string("(typeof window.FB_ORIGIN_LON==='number'?window.FB_ORIGIN_LON:'').toString()");
  if(so&&*so) w3_O.lon=atof(so);
  /* Simulated-UTC override (Unix seconds; 0/unset = real time): pins a reproducible night/dusk sky. */
  const char*su=emscripten_run_script_string("(typeof window.FB_SIM_UTC==='number'?window.FB_SIM_UTC:0).toString()");
  w3_sim_utc=atof(su);
  snprintf(tiles_url,n,"%s",emscripten_run_script_string("(window.FB_TILES_URL||'').toString()"));
}

/* Open the telemetry/control WebSocket back to the server that served the page. */
static void net_init(void){
  char url[256]; const char*host=emscripten_run_script_string("location.host");
  snprintf(url,sizeof url,"ws://%s/ws",host&&*host?host:"127.0.0.1:8080");
  EmscriptenWebSocketCreateAttributes attr={url,NULL,EM_TRUE}; ws=emscripten_websocket_new(&attr);
  emscripten_websocket_set_onopen_callback(ws,0,on_open);
  emscripten_websocket_set_onclose_callback(ws,0,on_close);
  emscripten_websocket_set_onmessage_callback(ws,0,on_msg);
}

/* Load the star catalogue once at startup: fetch the 4 HYG magnitude bands (~53 KB total, universal
 * and static) into one buffer -- concatenated they stay globally mag-sorted -- and decode into
 * w3_stars. A failed fetch leaves the catalogue empty (a blank night sky), never blocks startup. */
static void stars_load_from_tiles(void){
  const int cap = 96*1024;
  uint8_t *buf = (uint8_t*)malloc((size_t)cap); if(!buf) return;
  int off = 0, ok = 1;
  for(int b=0;b<4;b++){
    int n = fb_fetch_stars(b, buf+off, cap-off);
    if(n<0){ printf("[cc] star band %d fetch failed\n", b); ok=0; break; }
    off += n;
  }
  if(ok){ int ns = w3_stars_load(buf, off); printf("[cc] star catalogue: %d stars, %d bytes\n", ns, off); }
  free(buf);
}

int main(void){
  char tiles_url[160];
  cfg_from_js(tiles_url, sizeof tiles_url);

  SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);   /* WebGL2 — VideoFrame texture upload */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
  win=SDL_CreateWindow("FlightBox",0,0,WIN_W,WIN_H,SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(win);
  emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),"EXT_texture_filter_anisotropic");

  { double he=fb_fetch_elev(w3_O.lat,w3_O.lon);   /* seed camera lift before streaming; else spawn is underground */
    if(he>-1e8){ w3_seed_yoff((float)he); printf("[cc] origin ground %.1f m (/elev), camera seeded\n",he); }
    else printf("[cc] /elev unreachable at startup — lift waits for the origin tile\n"); }
  /* Stream every tile on demand from fb-tiles — works at ANY origin on earth. */
  if(tiles_url[0] && world3d_tiles_open(tiles_url,w3_O.lat,w3_O.lon))
    printf("[cc] tiles: streaming from %s, origin %.4f/%.4f\n",tiles_url,w3_O.lat,w3_O.lon);
  else printf("[cc] no tiles url or open failed — procedural fallback\n");
  world3d_init();
  stars_load_from_tiles();

  fb_present_init(win);
  net_init();
  emscripten_set_main_loop(frame,0,1);
  return 0;
}
