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
 * All 3D + HUD live in world3d.h so this browser view matches the native renderer.
 * Keys: arrows roll/pitch, A/D yaw, W/S throttle, ENTER arm, L drop link,
 * TAB ground = OSM render <-> aerial photo (F = fullscreen, both handled in index.html). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/html5.h>
#define W3_USE_OSM
#include "world3d.h"

/* WebGL2 / GLES3 functions for MSAA (not declared in the ES2 header) */
#define GL_RGBA8 0x8058
#define GL_MAX_SAMPLES 0x8D57
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
extern void glRenderbufferStorageMultisample(GLenum,GLsizei,GLenum,GLsizei,GLsizei);
extern void glBlitFramebuffer(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);

/* wasm-dvd-gl approach: render + encode at the real CAMERA resolution (below the display),
 * then bilinear-upscale to the canvas. Our hardware camera is a Caddx Ratel 2 (1/1.8"
 * Starlight, 164° FOV, NTSC 16:9, 60 Hz) — analog composite ~480 lines, 16:9. We render
 * 16:9 at NTSC line count (480) and encode it (H.264 stands in for the analog link's
 * softness/artifacts); NTSC's 60 Hz matches a 60 fps render (smooth, no cadence judder).
 * MSAA cleans the edges at render res; the HUD is drawn crisp at full display res on top. */
#define WIN_W 1280          /* display / canvas (720p, 16:9) */
#define WIN_H 720
#define CAM_W 848           /* "camera": NTSC 480-line, 16:9 */
#define CAM_H 480

/* ===================== WebCodecs H.264 encode -> decode ===================== */
EM_JS(int, fb_codec_init, (int w, int h), {
  if (typeof VideoEncoder === 'undefined' || typeof VideoDecoder === 'undefined') {
    console.warn('[fb] WebCodecs nicht verfuegbar — Rohbild ohne Video-Artefakte.'); return 0;
  }
  const S = { ready:false, frame:null, n:0 }; Module.__fb = S;
  S.decoder = new VideoDecoder({ output:(f)=>{ if(S.frame) S.frame.close(); S.frame=f; },
                                 error:(e)=>console.error('[fb] dec',e) });
  S.encoder = new VideoEncoder({
    output:(chunk,meta)=>{ if(meta && meta.decoderConfig && S.decoder.state==='unconfigured') S.decoder.configure(meta.decoderConfig);
                           if(S.decoder.state==='configured') S.decoder.decode(chunk); },
    error:(e)=>console.error('[fb] enc',e) });
  try {
    S.encoder.configure({ codec:'avc1.42001f', width:w, height:h,
      bitrate: 1500000, framerate: 60, latencyMode:'realtime', avc:{format:'avc'} });
  } catch(e){ console.error('[fb] enc.configure', e); return 0; }
  S.ready = true; console.log('[fb] WebCodecs bereit', w+'x'+h); return 1;
});
EM_JS(void, fb_codec_push, (int ptr, int w, int h, double ts), {
  const S = Module.__fb; if(!S||!S.ready) return;
  if(S.encoder.encodeQueueSize > 2) return;                 /* backpressure */
  let vf; try { vf = new VideoFrame(HEAPU8.subarray(ptr, ptr+w*h*4),
    { format:'RGBA', codedWidth:w, codedHeight:h, timestamp:ts }); } catch(e){ return; }
  const key = (S.n % 50) === 0; S.n++;
  try { S.encoder.encode(vf, { keyFrame:key }); } catch(e){}
  vf.close();
});
EM_JS(int, fb_codec_upload, (int texId), {
  const S = Module.__fb; if(!S||!S.frame) return 0;
  const tex = GL.textures[texId]; if(!tex) return 0;
  GLctx.bindTexture(GLctx.TEXTURE_2D, tex);
  try { GLctx.texImage2D(GLctx.TEXTURE_2D,0,GLctx.RGBA,GLctx.RGBA,GLctx.UNSIGNED_BYTE, S.frame); }
  catch(e){ return 0; }
  return 1;
});

static SDL_Window *win;
static EMSCRIPTEN_WEBSOCKET_T ws; static int ws_open=0;
static telem_packet_t telem; static int have_telem=0;
static SDL_GameController *pad=NULL;
static double g_olat=52.045, g_olon=9.385;
static GLuint vid_fbo,vid_tex,vid_depth,codec_tex,pres_prog,quad_vbo;
static GLuint msaa_fbo,msaa_color,msaa_depth;   /* multisample render target (antialiasing) */
static GLint pr_pos,pr_uv,pr_tex,pr_flip;
static unsigned char *readback; static int codec_ready=0; static long frame_no=0;

static const char*PVS="attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;"
 "void main(){ gl_Position=vec4(aPos,0.0,1.0); vUV=aUV; }";
static const char*PFS="precision mediump float; varying vec2 vUV; uniform sampler2D uTex; uniform float uFlip;"
 "void main(){ vec2 c=vec2(vUV.x, uFlip>0.5?1.0-vUV.y:vUV.y); gl_FragColor=texture2D(uTex,c); }";

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

/* present a texture full-canvas (upscale). flip=1 for the top-down decoded VideoFrame. */
static void present(GLuint tex,float flip){
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(0,0,WIN_W,WIN_H); glDisable(GL_DEPTH_TEST);
  glUseProgram(pres_prog);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,tex); glUniform1i(pr_tex,0); glUniform1f(pr_flip,flip);
  glBindBuffer(GL_ARRAY_BUFFER,quad_vbo);
  glVertexAttribPointer(pr_pos,2,GL_FLOAT,GL_FALSE,16,0); glVertexAttribPointer(pr_uv,2,GL_FLOAT,GL_FALSE,16,(void*)8);
  glEnableVertexAttribArray(pr_pos); glEnableVertexAttribArray(pr_uv);
  glDrawArrays(GL_TRIANGLES,0,6);
}

static void frame(void){
  SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_CONTROLLERDEVICEADDED&&!pad) pad=SDL_GameControllerOpen(e.cdevice.which);
  send_control();
  /* Game-engine style: telemetry (the state) arrives faster than we render (100 Hz vs
   * 60 fps), so we just sample the LATEST pose each frame — smooth motion, zero added
   * latency (no interpolation delay). */
  if(have_telem){
    double lat = g_olat + (double)telem.y/111320.0;
    double lon = g_olon + (double)telem.x/(111320.0*cos(g_olat*M_PI/180.0));
    world3d_stream(lat, lon);
  }
  /* 1) render the aircraft camera into the MULTISAMPLE FBO (antialiased) */
  glBindFramebuffer(GL_FRAMEBUFFER, msaa_fbo);
  world3d_render_scene(&telem, CAM_W, CAM_H, have_telem);
  /* 1b) resolve MSAA -> single-sample vid_fbo (vid_tex) */
  glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vid_fbo);
  glBlitFramebuffer(0,0,CAM_W,CAM_H, 0,0,CAM_W,CAM_H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  /* 2) encode -> decode (the lossy video link) */
  int use_codec=0;
  if(codec_ready){
    glBindFramebuffer(GL_FRAMEBUFFER, vid_fbo);
    glReadPixels(0,0,CAM_W,CAM_H,GL_RGBA,GL_UNSIGNED_BYTE,readback);
    fb_codec_push((int)(intptr_t)readback, CAM_W, CAM_H, emscripten_get_now()*1000.0);  /* real-time µs timestamp */
    if(fb_codec_upload((int)codec_tex)) use_codec=1;
  }
  frame_no++;
  /* 3) present the received video upscaled, then 4) HUD crisp on top.
   * Both paths are GL-bottom-up (the codec's readback->VideoFrame double-flip cancels),
   * so no V-flip is needed. */
  present(use_codec?codec_tex:vid_tex, 0.0f);
  world3d_render_hud(&telem, WIN_W, WIN_H, have_telem);
  SDL_GL_SwapWindow(win);
}

int main(void){
  const char*sl=emscripten_run_script_string("(window.FB_ORIGIN_LAT||0).toString()");
  double la=atof(sl); if(la!=0) g_olat=la;
  const char*so=emscripten_run_script_string("(window.FB_ORIGIN_LON||0).toString()");
  double lo=atof(so); if(lo!=0) g_olon=lo;
  /* emscripten_run_script_string returns a REUSED buffer — copy before the next call, or both
   * pointers end up showing the last value (that bug once put the whole world at 9.385,9.385). */
  char tiles_url[160];
  snprintf(tiles_url,sizeof tiles_url,"%s",
           emscripten_run_script_string("(window.FB_TILES_URL||'').toString()"));

  SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);   /* WebGL2 — VideoFrame texture upload */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
  win=SDL_CreateWindow("FlightBox",0,0,WIN_W,WIN_H,SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(win);

  emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),"EXT_texture_filter_anisotropic");

  /* Preferred: stream every tile on demand from fb-tiles — works at ANY origin on earth.
   * Fallback: the legacy preloaded region archive (Hameln only). */
  if(tiles_url[0] && world3d_tiles_open(tiles_url,g_olat,g_olon))
    printf("[cc] tiles: streaming from %s, origin %.4f/%.4f\n",tiles_url,g_olat,g_olon);
  else if(world3d_osm_open("/hameln.pmtiles","/hameln_terrain.pmtiles",g_olat,g_olon))
    printf("[cc] tiles: legacy preloaded archive (Hameln), origin %.4f/%.4f\n",g_olat,g_olon);
  else printf("[cc] osmmesh open FAILED — procedural fallback\n");
  world3d_init();

  /* video FBO (colour texture + depth), decoded-frame texture, present shader + quad */
  vid_tex = 0; glGenTextures(1,&vid_tex); glBindTexture(GL_TEXTURE_2D,vid_tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,CAM_W,CAM_H,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenRenderbuffers(1,&vid_depth); glBindRenderbuffer(GL_RENDERBUFFER,vid_depth);
  glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,CAM_W,CAM_H);
  glGenFramebuffers(1,&vid_fbo); glBindFramebuffer(GL_FRAMEBUFFER,vid_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,vid_tex,0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,vid_depth);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  /* multisample render FBO (4x MSAA) — the scene renders here, then resolves to vid_fbo */
  { GLint maxs=1; glGetIntegerv(GL_MAX_SAMPLES,&maxs); int samp=maxs<4?maxs:4; if(samp<1)samp=1;
    glGenRenderbuffers(1,&msaa_color); glBindRenderbuffer(GL_RENDERBUFFER,msaa_color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,samp,GL_RGBA8,CAM_W,CAM_H);
    glGenRenderbuffers(1,&msaa_depth); glBindRenderbuffer(GL_RENDERBUFFER,msaa_depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,samp,GL_DEPTH_COMPONENT16,CAM_W,CAM_H);
    glGenFramebuffers(1,&msaa_fbo); glBindFramebuffer(GL_FRAMEBUFFER,msaa_fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,msaa_color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,msaa_depth);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    printf("[cc] MSAA %dx\n",samp); }
  codec_tex=0; glGenTextures(1,&codec_tex); glBindTexture(GL_TEXTURE_2D,codec_tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,CAM_W,CAM_H,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  pres_prog=w3_prog(PVS,PFS); pr_pos=glGetAttribLocation(pres_prog,"aPos"); pr_uv=glGetAttribLocation(pres_prog,"aUV");
  pr_tex=glGetUniformLocation(pres_prog,"uTex"); pr_flip=glGetUniformLocation(pres_prog,"uFlip");
  const float quad[]={ -1,-1, 0,0,  1,-1, 1,0,  1,1, 1,1,   -1,-1, 0,0,  1,1, 1,1,  -1,1, 0,1 };
  glGenBuffers(1,&quad_vbo); glBindBuffer(GL_ARRAY_BUFFER,quad_vbo); glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STATIC_DRAW);
  readback=(unsigned char*)malloc((size_t)CAM_W*CAM_H*4);
  codec_ready=fb_codec_init(CAM_W,CAM_H);
  printf("[cc] video codec %s\n", codec_ready?"ON (H.264 link)":"OFF (raw upscale)");

  char url[256]; const char*host=emscripten_run_script_string("location.host");
  snprintf(url,sizeof url,"ws://%s/ws",host&&*host?host:"127.0.0.1:8080");
  EmscriptenWebSocketCreateAttributes attr={url,NULL,EM_TRUE}; ws=emscripten_websocket_new(&attr);
  emscripten_websocket_set_onopen_callback(ws,0,on_open);
  emscripten_websocket_set_onclose_callback(ws,0,on_close);
  emscripten_websocket_set_onmessage_callback(ws,0,on_msg);
  emscripten_set_main_loop(frame,0,1);
  return 0;
}
