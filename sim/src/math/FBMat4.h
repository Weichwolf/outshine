/* FlightBox renderer — 4x4 matrix / 3-vector maths (column-major, OpenGL convention).
 *
 * Split out of world3d.h because it is the one part of the renderer that needs no GL context:
 * pure float maths, so it can be asserted directly instead of being judged by looking at pixels.
 * A wrong sign here does not crash — it quietly mirrors the world or turns the camera inside out,
 * which is exactly the class of bug a unit test catches and an eyeball does not.
 *
 * Column-major, matching glUniformMatrix4fv(..., GL_FALSE, m): element m[c*4+r] is column c,
 * row r. Multiplying by a column vector v gives m*v.
 */
#ifndef FBMAT4_H
#define FBMAT4_H
#include <math.h>
#include <string.h>

static void m_identity(float *m) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}

static void m_mul(float *o, const float *a, const float *b) {
  float r[16];
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) {
      float s = 0;
      for (int k = 0; k < 4; k++)
        s += a[k * 4 + rr] * b[c * 4 + k];
      r[c * 4 + rr] = s;
    }
  memcpy(o, r, 64);
}

/* Right-handed perspective, looking down -z, mapping the frustum to clip space. */
/* REVERSED-Z perspective: near maps to NDC z=+1 (window depth 1.0), far to -1 (window 0.0) — the
 * standard zn<->zf swap in the z-row. Paired with glClearDepthf(0) + glDepthFunc(GL_GEQUAL) and the
 * 32-bit float depth buffer (present.h), the projection's 1/z curve cancels the float mantissa's
 * distribution, giving near-uniform precision across a 0.01 m..240 km range where plain depth
 * z-fights distant terrain into flicker (lod.h). x/y (m[0],m[5]) and w (m[11]) are unchanged, so
 * screen projection, the HUD's manual projection, and frustum extraction are unaffected. */
static void m_persp(float *m, float fovy, float asp, float zn, float zf) {
  float f = 1.f / tanf(fovy * 0.5f);
  memset(m, 0, 64);
  m[0] = f / asp;
  m[5] = f;
  m[10] = (zn + zf) / (zf - zn);
  m[11] = -1;
  m[14] = (2 * zn * zf) / (zf - zn);
}

static void v_norm(float *v) {
  float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 1e-6f) {
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
  }
}

static void v_cross(float *o, const float *a, const float *b) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}

/* World -> view. Camera at eye, looking at ctr, with up roughly `up`. */
static void m_lookat(float *m, const float *eye, const float *ctr, const float *up) {
  float f[3] = {ctr[0] - eye[0], ctr[1] - eye[1], ctr[2] - eye[2]};
  v_norm(f);
  float s[3];
  v_cross(s, f, up);
  v_norm(s);
  float u[3];
  v_cross(u, s, f);
  m_identity(m);
  m[0] = s[0];
  m[4] = s[1];
  m[8] = s[2];
  m[1] = u[0];
  m[5] = u[1];
  m[9] = u[2];
  m[2] = -f[0];
  m[6] = -f[1];
  m[10] = -f[2];
  m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
  m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
  m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
}

#endif /* FBMAT4_H */
