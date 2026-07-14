/* FlightBox — Command Center (C -> WASM), 3D world camera (Teil B).
 * WebGL/GLES2, all in C. The "camera image" is the REAL world streamed live from
 * osmmesh (OSM Shortbread buildings + Copernicus terrain PMTiles) around the
 * aircraft's GPS position, rendered from its live pose (ENU position + attitude)
 * delivered over WebSocket telemetry. A 2D HUD (home steerpoint / glideslope /
 * readouts) is drawn on top. Game controller / keyboard send correction requests.
 *
 * All rendering + HUD live in the shared world3d.h so this browser view is pixel-
 * identical to the native offscreen renderer used for headless visual evaluation.
 *
 * Keys: arrows roll/pitch, A/D yaw, W/S throttle, ENTER arm, L drop link. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/websocket.h>
#define W3_USE_OSM
#include "world3d.h"     /* matrix + shaders + HUD + world3d_init/stream/render + osmmesh */

#define WIN_W 800
#define WIN_H 600

static SDL_Window *win;
static EMSCRIPTEN_WEBSOCKET_T ws; static int ws_open=0;
static telem_packet_t telem; static int have_telem=0;
static SDL_GameController *pad=NULL;
static double g_olat=52.045, g_olon=9.385;    /* ENU origin = home (from server config) */

/* ---------------- websocket ---------------- */
static EM_BOOL on_open(int t,const EmscriptenWebSocketOpenEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=1;return 1;}
static EM_BOOL on_close(int t,const EmscriptenWebSocketCloseEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=0;return 1;}
static EM_BOOL on_msg(int t,const EmscriptenWebSocketMessageEvent*e,void*u){(void)t;(void)u;
  if(e->numBytes<4) return 1; uint32_t mg; memcpy(&mg,e->data,4);
  if(mg==FB_MAGIC_TELEM && e->numBytes==sizeof(telem_packet_t)){ memcpy(&telem,e->data,sizeof telem); have_telem=1; }
  return 1;
}

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
  static int seen_telem=0;
  if(have_telem){
    if(!seen_telem){ seen_telem=1; printf("[cc] first telemetry: alt=%.0f x=%.0f y=%.0f\n",telem.alt,telem.x,telem.y); }
    /* aircraft geographic position from ENU pose (east=x, north=y) -> stream tiles */
    double lat = g_olat + (double)telem.y/111320.0;
    double lon = g_olon + (double)telem.x/(111320.0*cos(g_olat*M_PI/180.0));
    world3d_stream(lat, lon);
  }
  world3d_render(&telem, WIN_W, WIN_H, have_telem);
  static int checked=0; if(!checked){ checked=1; GLenum ge=glGetError(); if(ge) printf("[cc] GL error after first render: 0x%x\n",ge); else printf("[cc] first render OK\n"); }
  SDL_GL_SwapWindow(win);
}

int main(void){
  /* origin (home) from server config, injected as window.FB_ORIGIN_LAT/LON.
   * NOTE: emscripten_run_script_string returns a REUSED buffer, so each result
   * must be consumed (atof) before the next call — don't hold two pointers. */
  double la=atof(emscripten_run_script_string("(window.FB_ORIGIN_LAT||0).toString()"));
  if(la!=0) g_olat=la;
  double lo=atof(emscripten_run_script_string("(window.FB_ORIGIN_LON||0).toString()"));
  if(lo!=0) g_olon=lo;

  SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
  win=SDL_CreateWindow("FlightBox",0,0,WIN_W,WIN_H,SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(win);

  /* PMTiles are preloaded into MEMFS at build time (--preload-file). */
  if(world3d_osm_open("/hameln.pmtiles","/hameln_terrain.pmtiles",g_olat,g_olon))
    printf("[cc] osmmesh open OK, origin %.4f/%.4f\n",g_olat,g_olon);
  else
    printf("[cc] osmmesh open FAILED — falling back to procedural world\n");
  world3d_init();

  char url[256]; const char*host=emscripten_run_script_string("location.host");
  snprintf(url,sizeof url,"ws://%s/ws",host&&*host?host:"127.0.0.1:8080");
  EmscriptenWebSocketCreateAttributes attr={url,NULL,EM_TRUE}; ws=emscripten_websocket_new(&attr);
  emscripten_websocket_set_onopen_callback(ws,0,on_open);
  emscripten_websocket_set_onclose_callback(ws,0,on_close);
  emscripten_websocket_set_onmessage_callback(ws,0,on_msg);
  emscripten_set_main_loop(frame,0,1);
  return 0;
}
