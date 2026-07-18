#ifndef FB_CC_PRESENT_H
#define FB_CC_PRESENT_H
/* The present pipeline: scene -> (EVS video link | SVS direct) -> HUD -> canvas.
 *
 * Two resolutions. The canvas backbuffer tracks the DISPLAY: client size x devicePixelRatio, so
 * fullscreen renders at the native screen resolution (present size pw x ph, re-synced each frame).
 *   - SVS (synthetic, no codec): rendered AT present size, resolved 1:1 to the canvas -> crisp.
 *   - EVS (real camera): rendered at CAM size, through the H.264 link, upscaled to the canvas -> the
 *     lossy 5.8 GHz downlink's softness/artifacts are the truth and stay.
 *   - HUD: always at present size, glow-composited on top -> crisp at any resolution.
 *
 * Owns every render target + shader; cc.c calls only fb_present_init / fb_present_frame. Include
 * after world3d.h, constants.h, codec.h. */

#define CAM_W 848   /* "camera": Caddx Ratel 2, NTSC 480-line, 16:9 -- the EVS render+encode size */
#define CAM_H 480

static const char *PVS = "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;"
 "void main(){ gl_Position=vec4(aPos,0.0,1.0); vUV=aUV; }";
static const char *PFS = "precision mediump float; varying vec2 vUV; uniform sampler2D uTex; uniform float uFlip;"
 "void main(){ vec2 c=vec2(vUV.x, uFlip>0.5?1.0-vUV.y:vUV.y); gl_FragColor=texture2D(uTex,c); }";
static const char *HGVS = "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;"
 "void main(){ gl_Position=vec4(aPos,0.0,1.0); vUV=aUV; }";
static const char *HGFS = "precision mediump float; varying vec2 vUV; uniform sampler2D uHud; uniform vec2 uTexel;"
 "void main(){ vec4 c=texture2D(uHud,vUV); vec2 t=uTexel*1.4;"
 "  vec4 g=texture2D(uHud,vUV+vec2(t.x,0.0))+texture2D(uHud,vUV+vec2(-t.x,0.0))"
 "        +texture2D(uHud,vUV+vec2(0.0,t.y))+texture2D(uHud,vUV+vec2(0.0,-t.y))"
 "        +texture2D(uHud,vUV+t)+texture2D(uHud,vUV-t)"
 "        +texture2D(uHud,vUV+vec2(t.x,-t.y))+texture2D(uHud,vUV+vec2(-t.x,t.y));"
 "  g*=0.125; float ga=g.a*0.18;"
 "  vec3 col = c.a>0.01 ? c.rgb : g.rgb/max(g.a,0.001);"
 "  gl_FragColor=vec4(col, clamp(c.a+ga,0.0,1.0)); }";

static SDL_Window *pr_win;
static int pr_samples = 1;
static int pw = 0, ph = 0;                                  /* current present (display) size */
static GLuint quad_vbo, pres_prog, hud_prog;
static GLint pr_pos, pr_uv, pr_tex, pr_flip;
static GLint hu_pos, hu_uv, hu_tex, hu_texel;
static GLuint cam_msaa_fbo, cam_msaa_c, cam_msaa_d, cam_vid_fbo, cam_vid_tex, codec_tex;  /* EVS, fixed CAM */
static unsigned char *readback;
static int codec_ready = 0;
static GLuint p_msaa_fbo, p_msaa_c, p_msaa_d, p_vid_fbo, p_vid_tex;  /* SVS scene MSAA + resolve @ present */
static GLuint hud_msaa_fbo, hud_msaa_c, hud_fbo, hud_tex;   /* HUD MSAA + resolve @ present size */

static GLuint pr_tex2d(int w, int h) {
  GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

/* Multisample scene FBO (colour + depth). 24-bit depth: with the far plane at 240 km, 16 bits
 * z-fight the distant terrain into confetti (~760 m at 10 km); 24 give 0.3 m. WebGL1 lacks it -> fall back. */
static void pr_mk_scene(GLuint *fbo, GLuint *col, GLuint *dep, int w, int h) {
  glGenRenderbuffers(1, col); glBindRenderbuffer(GL_RENDERBUFFER, *col);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, pr_samples, GL_RGBA8, w, h);
  glGenRenderbuffers(1, dep); glBindRenderbuffer(GL_RENDERBUFFER, *dep);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, pr_samples, GL_DEPTH_COMPONENT24, w, h);
  if (glGetError() != GL_NO_ERROR)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, pr_samples, GL_DEPTH_COMPONENT16, w, h);
  glGenFramebuffers(1, fbo); glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *col);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *dep);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* Present-size targets (SVS scene + HUD). Freed and rebuilt when the display size changes. */
static void pr_free_display(void) {
  if (!p_msaa_fbo) return;
  glDeleteFramebuffers(1, &p_msaa_fbo); glDeleteRenderbuffers(1, &p_msaa_c); glDeleteRenderbuffers(1, &p_msaa_d);
  glDeleteFramebuffers(1, &p_vid_fbo); glDeleteTextures(1, &p_vid_tex);
  glDeleteFramebuffers(1, &hud_msaa_fbo); glDeleteRenderbuffers(1, &hud_msaa_c);
  glDeleteFramebuffers(1, &hud_fbo); glDeleteTextures(1, &hud_tex);
}
static void pr_alloc_display(int w, int h) {
  pr_mk_scene(&p_msaa_fbo, &p_msaa_c, &p_msaa_d, w, h);
  p_vid_tex = pr_tex2d(w, h);
  glGenFramebuffers(1, &p_vid_fbo); glBindFramebuffer(GL_FRAMEBUFFER, p_vid_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_vid_tex, 0);
  glGenRenderbuffers(1, &hud_msaa_c); glBindRenderbuffer(GL_RENDERBUFFER, hud_msaa_c);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, pr_samples, GL_RGBA8, w, h);
  glGenFramebuffers(1, &hud_msaa_fbo); glBindFramebuffer(GL_FRAMEBUFFER, hud_msaa_fbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, hud_msaa_c);
  hud_tex = pr_tex2d(w, h);
  glGenFramebuffers(1, &hud_fbo); glBindFramebuffer(GL_FRAMEBUFFER, hud_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hud_tex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* getBoundingClientRect = border-BOX (stable): the canvas has no CSS width, so its layout width IS
 * the backbuffer width attribute. clientWidth would EXCLUDE the 1px border, so feeding it back would
 * ratchet the backbuffer down 2px/frame to nothing -- the rect includes the border and stays put. */
EM_JS(void, pr_display_px, (int *wOut, int *hOut), {
  const c = Module.canvas, r = c.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  HEAP32[wOut>>2] = Math.max(1, Math.round((r.width  || c.width)  * dpr));
  HEAP32[hOut>>2] = Math.max(1, Math.round((r.height || c.height) * dpr));
})

static void pr_sync_size(void) {
  int w, h; pr_display_px(&w, &h);
  if (w < 16) w = 16; if (h < 16) h = 16;
  if (w > 3840) w = 3840; if (h > 3840) h = 3840;      /* backstop against a pathological dpr */
  if (w > pw-2 && w < pw+2 && h > ph-2 && h < ph+2) return;   /* hysteresis: ignore sub-pixel jitter */
  pw = w; ph = h;
  emscripten_set_canvas_element_size("#canvas", pw, ph);
  pr_free_display(); pr_alloc_display(pw, ph);
  printf("[cc] present %dx%d (SVS+HUD render at this; EVS stays %dx%d)\n", pw, ph, CAM_W, CAM_H);
}

/* Draw tex full-canvas at present size (EVS upscale). flip=1 for the top-down decoded VideoFrame. */
static void pr_present(GLuint tex, float flip) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, pw, ph); glDisable(GL_DEPTH_TEST);
  glUseProgram(pres_prog);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(pr_tex, 0); glUniform1f(pr_flip, flip);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glVertexAttribPointer(pr_pos, 2, GL_FLOAT, GL_FALSE, 16, 0);
  glVertexAttribPointer(pr_uv, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
  glEnableVertexAttribArray(pr_pos); glEnableVertexAttribArray(pr_uv);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void fb_present_init(SDL_Window *win) {
  pr_win = win;
  { GLint m = 1; glGetIntegerv(GL_MAX_SAMPLES, &m); pr_samples = m < 4 ? m : 4; if (pr_samples < 1) pr_samples = 1; }
  pres_prog = w3_prog(PVS, PFS);
  pr_pos = glGetAttribLocation(pres_prog, "aPos"); pr_uv = glGetAttribLocation(pres_prog, "aUV");
  pr_tex = glGetUniformLocation(pres_prog, "uTex"); pr_flip = glGetUniformLocation(pres_prog, "uFlip");
  hud_prog = w3_prog(HGVS, HGFS);
  hu_pos = glGetAttribLocation(hud_prog, "aPos"); hu_uv = glGetAttribLocation(hud_prog, "aUV");
  hu_tex = glGetUniformLocation(hud_prog, "uHud"); hu_texel = glGetUniformLocation(hud_prog, "uTexel");
  const float quad[] = { -1,-1, 0,0,  1,-1, 1,0,  1,1, 1,1,   -1,-1, 0,0,  1,1, 1,1,  -1,1, 0,1 };
  glGenBuffers(1, &quad_vbo); glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);

  cam_vid_tex = pr_tex2d(CAM_W, CAM_H);
  glGenFramebuffers(1, &cam_vid_fbo); glBindFramebuffer(GL_FRAMEBUFFER, cam_vid_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cam_vid_tex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  pr_mk_scene(&cam_msaa_fbo, &cam_msaa_c, &cam_msaa_d, CAM_W, CAM_H);
  codec_tex = pr_tex2d(CAM_W, CAM_H);
  readback = (unsigned char*)malloc((size_t)CAM_W * CAM_H * 4);
  codec_ready = fb_codec_init(CAM_W, CAM_H);
  printf("[cc] MSAA %dx, video codec %s\n", pr_samples, codec_ready ? "ON (H.264 link)" : "OFF (raw upscale)");

  pw = ph = 0; pr_sync_size();      /* first display alloc */
}

static void fb_present_frame(const telem_packet_t *telem, int have) {
  pr_sync_size();
  int evs = codec_ready && (w3_ground.mode == W3_GROUND_PHOTO);
  if (evs) {
    glBindFramebuffer(GL_FRAMEBUFFER, cam_msaa_fbo);
    world3d_render_scene(telem, CAM_W, CAM_H, have);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cam_msaa_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cam_vid_fbo);
    glBlitFramebuffer(0,0,CAM_W,CAM_H, 0,0,CAM_W,CAM_H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, cam_vid_fbo);
    glReadPixels(0,0,CAM_W,CAM_H, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    fb_codec_push((int)(intptr_t)readback, CAM_W, CAM_H, emscripten_get_now()*1000.0);
    int up = fb_codec_upload((int)codec_tex);
    pr_present(up ? codec_tex : cam_vid_tex, 0.0f);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, p_msaa_fbo);
    world3d_render_scene(telem, pw, ph, have);          /* SVS at present size */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, p_msaa_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, p_vid_fbo);
    glBlitFramebuffer(0,0,pw,ph, 0,0,pw,ph, GL_COLOR_BUFFER_BIT, GL_NEAREST);   /* resolve MSAA -> texture */
    pr_present(p_vid_tex, 0.0f);                         /* 1:1 to canvas (the default FB may be MSAA -> no direct blit) */
  }
  glBindFramebuffer(GL_FRAMEBUFFER, hud_msaa_fbo);
  glViewport(0,0,pw,ph); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
  world3d_render_hud(telem, pw, ph, have);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, hud_msaa_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hud_fbo);
  glBlitFramebuffer(0,0,pw,ph, 0,0,pw,ph, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0,0,pw,ph); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(hud_prog);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hud_tex); glUniform1i(hu_tex, 0);
  glUniform2f(hu_texel, 1.0f/pw, 1.0f/ph);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glVertexAttribPointer(hu_pos, 2, GL_FLOAT, GL_FALSE, 16, 0);
  glVertexAttribPointer(hu_uv, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
  glEnableVertexAttribArray(hu_pos); glEnableVertexAttribArray(hu_uv);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisable(GL_BLEND);
  SDL_GL_SwapWindow(pr_win);
}

#endif
