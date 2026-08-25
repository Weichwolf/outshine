#ifndef OUTSHINE_CONTENT_SHADE_CAMERABASIS_H
#define OUTSHINE_CONTENT_SHADE_CAMERABASIS_H

#include "Units.h"
#include "Mat4.h"
#include "Geodesy.h"
#include <cmath>
#include <math.h>

namespace outshine {

struct CameraBasis {
  float f[3];
  float sr[3];
  float up[3];
  float mvp[16];
};

inline void CameraAxes(float yawDeg, float pitchDeg, float rollDeg, float f[3], float sr[3],
                         float up[3]) {
  const float RAD = (float)kPi / 180.f;
  float yaw = yawDeg * RAD, pitch = pitchDeg * RAD, roll = rollDeg * RAD;

  f[0] = cosf(pitch) * sinf(yaw);
  f[1] = sinf(pitch);
  f[2] = -cosf(pitch) * cosf(yaw);

  float wup[3] = {0, 1, 0}, s[3], u[3];
  Vec3Cross(s, f, wup);
  Vec3Normalize(s);
  Vec3Cross(u, s, f);

  for (int i = 0; i < 3; i++) {
    up[i] = u[i] * cosf(roll) + s[i] * sinf(roll);
    sr[i] = s[i] * cosf(roll) - u[i] * sinf(roll);
  }
}

inline CameraBasis CameraBasisFrom(float yawDeg, float pitchDeg, float rollDeg, const float eye[3],
                                       float fovDeg, float aspect, float znear, float zfar) {
  const float RAD = (float)kPi / 180.f;
  CameraBasis c;
  CameraAxes(yawDeg, pitchDeg, rollDeg, c.f, c.sr, c.up);

  float ctr[3] = {eye[0] + c.f[0], eye[1] + c.f[1], eye[2] + c.f[2]};
  float view[16], proj[16];
  Mat4LookAt(view, eye, ctr, c.up);
  Mat4Perspective(proj, fovDeg * RAD, aspect, znear, zfar);
  Mat4Mul(c.mvp, proj, view);
  return c;
}

inline void CameraBasisEcef(double yawDeg, double pitchDeg, double rollDeg, double latDeg,
                              double lonDeg, double fwd[3], double right[3], double up[3]) {
  double yaw = yawDeg * kDeg2Rad, pitch = pitchDeg * kDeg2Rad, roll = rollDeg * kDeg2Rad;
  double fR[3] = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
  double wup[3] = {0, 1, 0}, s[3], u[3];
  s[0] = fR[1] * wup[2] - fR[2] * wup[1];
  s[1] = fR[2] * wup[0] - fR[0] * wup[2];
  s[2] = fR[0] * wup[1] - fR[1] * wup[0];
  { double l = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);

    if (l < 1e-12) {
      s[0] = std::cos(yaw); s[1] = 0.0; s[2] = std::sin(yaw);
    } else {
      s[0] /= l; s[1] /= l; s[2] /= l;
    } }
  u[0] = s[1] * fR[2] - s[2] * fR[1];
  u[1] = s[2] * fR[0] - s[0] * fR[2];
  u[2] = s[0] * fR[1] - s[1] * fR[0];
  double upR[3], srR[3];
  for (int i = 0; i < 3; i++) {
    upR[i] = u[i] * std::cos(roll) + s[i] * std::sin(roll);
    srR[i] = s[i] * std::cos(roll) - u[i] * std::sin(roll);
  }
  double E[3], N[3], U[3];
  EnuAxesEcef(latDeg, lonDeg, E, N, U);
  auto toEcef = [&](const double rv[3], double out[3]) {
    double e = rv[0], uu = rv[1], nn = -rv[2];
    for (int i = 0; i < 3; i++) out[i] = E[i] * e + N[i] * nn + U[i] * uu;
  };
  toEcef(fR, fwd);
  toEcef(srR, right);
  toEcef(upR, up);
}

inline void MvpTranslate(float out[16], const float vp[16], const float t[3]) {
  for (int i = 0; i < 12; i++)
    out[i] = vp[i];
  for (int r = 0; r < 4; r++)
    out[12 + r] = vp[r] * t[0] + vp[4 + r] * t[1] + vp[8 + r] * t[2] + vp[12 + r];
}

struct Frustum {
  float p[6][4];
};

inline Frustum FrustumFrom(const float mvp[16]) {
  Frustum fr;
  for (int a = 0; a < 3; a++)
    for (int j = 0; j < 4; j++) {
      float w = mvp[j * 4 + 3], c = mvp[j * 4 + a];
      fr.p[a * 2][j] = w + c;
      fr.p[a * 2 + 1][j] = w - c;
    }
  return fr;
}

inline int AabbVisible(const Frustum *fr, const float bmin[3], const float bmax[3]) {
  for (int k = 0; k < 6; k++) {
    const float *pl = fr->p[k];
    float d = pl[3] + pl[0] * (pl[0] >= 0 ? bmax[0] : bmin[0]) +
              pl[1] * (pl[1] >= 0 ? bmax[1] : bmin[1]) + pl[2] * (pl[2] >= 0 ? bmax[2] : bmin[2]);
    if (d < 0) return 0;
  }
  return 1;
}

}
#endif
