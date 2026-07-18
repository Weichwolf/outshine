/* FlightBox command center — invariants: numbers and declarations FIXED by physics, maths, or a
 * foreign API. Changing one makes the code WRONG, not merely different -- that is the test for what
 * belongs here. Tunables (knobs you can turn and it still runs correctly) do NOT live here; they
 * become runtime settings loaded from config. Requires a GL header (GLenum/GLsizei) already
 * included. */
#ifndef FB_CONSTANTS_H
#define FB_CONSTANTS_H

/* Metres per degree of latitude (~111.32 km): the local flat-earth scale that turns the aircraft's
 * ENU telemetry offset back into lat/lon around the origin. Longitude scales by cos(lat). */
#define FB_M_PER_DEG_LAT 111320.0

/* WebGL2 / GLES3 GL enums and entry points the ES2 header does not declare -- used for the MSAA
 * render target and its resolve blit. The hex values are fixed by the GL spec. */
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
extern void glRenderbufferStorageMultisample(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
extern void glBlitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                              GLenum);

#endif
