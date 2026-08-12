/* 4x4 matrix / 3-vector maths, column-major (element m[c*4+r] is column c, row r; m*v takes a column
 * vector) — the one part of the renderer that needs no GL context and can therefore be asserted
 * directly instead of judged by looking at pixels. */
#ifndef MAT4_H
#define MAT4_H
#include <math.h>
#include <string.h>

namespace outshine {

inline void Mat4Identity(float *m) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}

inline void Mat4Mul(float *o, const float *a, const float *b) {
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

/* Right-handed REVERSED-Z perspective: near maps to NDC z=+1, far to -1 (the zn<->zf swap in the
 * z-row). With GL_GEQUAL and a 32-bit float depth buffer the 1/z curve cancels the mantissa
 * distribution, giving near-uniform precision over 0.01 m..240 km. x/y and w are unchanged, so screen
 * projection, the HUD's manual projection and frustum extraction are unaffected. */
inline void Mat4Perspective(float *m, float fovy, float asp, float zn, float zf) {
  float f = 1.f / tanf(fovy * 0.5f);
  memset(m, 0, 64);
  m[0] = f / asp;
  m[5] = f;
  m[10] = (zn + zf) / (zf - zn);
  m[11] = -1;
  m[14] = (2 * zn * zf) / (zf - zn);
}

inline void Vec3Normalize(float *v) {
  float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 1e-6f) {
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
  }
}

inline void Vec3Cross(float *o, const float *a, const float *b) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}

/* World -> view. Camera at eye, looking at ctr, with up roughly `up`. */
inline void Mat4LookAt(float *m, const float *eye, const float *ctr, const float *up) {
  float f[3] = {ctr[0] - eye[0], ctr[1] - eye[1], ctr[2] - eye[2]};
  Vec3Normalize(f);
  float s[3];
  Vec3Cross(s, f, up);
  Vec3Normalize(s);
  float u[3];
  Vec3Cross(u, s, f);
  Mat4Identity(m);
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

} // namespace outshine
#endif /* MAT4_H */
