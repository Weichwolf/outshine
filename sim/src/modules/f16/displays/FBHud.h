/* FlightBox — WebGPU-side reuse of the WebGL HUD symbology (command_center/hud.h + max7456.h). Those
 * headers assume the GL context world3d.h supplies; here we stub the GL DRAW backend to no-ops (we
 * never call the GL draw functions — only w3_build_hud + mx_text, which are pure GEOMETRY), pull in
 * the real camera math, and reuse the MIL-STD-1787 symbology VERBATIM. FBRenderer's WebGPU backend
 * reads the filled buffers (w3_hud lines, w3_hudT triangles, mx_v textured glyphs) each frame.
 *
 * This is the "adapt the backend, not the symbology" split: the WebGL cc keeps FBHudSymbology.h/
 * FBMax7456.h with their real GL draws (via world3d.h); the WebGPU port includes THIS shim instead. */
#ifndef FBHUD_H
#define FBHUD_H

#include "FBMode.h"
#include "FBState.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* --- GL compat: no-op the draw backend so the symbology headers compile without a GL context. The
 * draw functions (world3d_render_hud, mx_render, mx_init) are never called here. The GCC-form pragmas
 * (Clang honors them too) suppress the "unused" noise from those stubbed-out backend statics. --- */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
typedef unsigned int GLuint;
typedef int GLint;
enum {
  GL_ARRAY_BUFFER = 0, GL_DYNAMIC_DRAW, GL_FLOAT, GL_FALSE, GL_TRIANGLES, GL_LINES, GL_DEPTH_TEST,
  GL_TEXTURE_2D, GL_LUMINANCE, GL_UNSIGNED_BYTE, GL_NEAREST, GL_TEXTURE_MIN_FILTER,
  GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_TEXTURE0
};
#define glViewport(...) ((void)0)
#define glDisable(...) ((void)0)
#define glEnable(...) ((void)0)
#define glUseProgram(...) ((void)0)
#define glUniform1i(...) ((void)0)
#define glUniform2f(...) ((void)0)
#define glEnableVertexAttribArray(...) ((void)0)
#define glDisableVertexAttribArray(...) ((void)0)
#define glBindBuffer(...) ((void)0)
#define glBufferData(...) ((void)0)
#define glVertexAttribPointer(...) ((void)0)
#define glDrawArrays(...) ((void)0)
#define glGenBuffers(...) ((void)0)
#define glGenTextures(...) ((void)0)
#define glBindTexture(...) ((void)0)
#define glActiveTexture(...) ((void)0)
#define glTexImage2D(...) ((void)0)
#define glTexParameteri(...) ((void)0)
#define glTexParameterf(...) ((void)0)
#define glGetAttribLocation(...) (0)
#define glGetUniformLocation(...) (0)
static inline GLuint w3_prog(const char *a, const char *b) { (void)a; (void)b; return 0; }
static struct { GLint pH, hScale, hPos, hCol, hVBO; } w3_gl;
static float w3_agl = 0.0f;
#define W3_FOV 80.0f
/* Ground source annunciator (SVS/EVS): FBRenderer::SetGroundMode writes this so the HUD reads the
 * REAL active source the TAB toggle selected (starts OSM -> "SVS"). */
enum { W3_GROUND_OSM = 0, W3_GROUND_PHOTO = 1 };
static struct { int mode; } w3_ground = {W3_GROUND_OSM};

#include "FBCamera.h"        /* real: w3_cam_from / w3_horizon_dip_rad (pure math) */
#include "FBMax7456.h"       /* reuse: MX_FONT atlas + mx_reset/mx_text (fills mx_v) */
#include "FBHudSymbology.h"  /* reuse: w3_build_hud symbology (fills w3_hud / w3_hudT) */
#pragma GCC diagnostic pop

#endif /* FBHUD_H */
