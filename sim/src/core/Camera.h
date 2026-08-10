/* The camera basis of ONE frame, as pure mathematics: attitude in, orthonormal basis + MVP out. No
 * GL, no globals, no side effects. The eye POSITION is deliberately not computed here — it depends on
 * the terrain and belongs to the tile side. fov/aspect/near/far are PARAMETERS, not the renderer's
 * constants, so this is testable against something other than the renderer's own configuration.
 *
 * Render space is ENU: E=+X, up=+Y, N=-Z; angles in degrees. */
#ifndef CAMERA_H
#define CAMERA_H

#include "Mat4.h"
#include "Geodesy.h"
#include <cmath>
#include <math.h>

namespace outshine {

struct CameraBasis {
  float f[3];    /* forward: where the nose points */
  float sr[3];   /* screen-right, AFTER roll */
  float up[3];   /* screen-up, AFTER roll */
  float mvp[16]; /* projection * view, column-major, ready for glUniformMatrix4fv */
};

/* Sphere mean radius: the terrain is on the ellipsoid, but the dip is a small-angle spherical result. */
constexpr float kEarthRadiusM = 6371000.0f;

/* acos(R/(R+h)): a level HUD horizon reads too HIGH aloft and must drop by exactly this to overlay
 * the real, curved-away one (MIL-STD-1787 conformal). */
inline float HorizonDipRad(float altM) {
  if (altM <= 0.f) return 0.f;
  return acosf(kEarthRadiusM / (kEarthRadiusM + altM));
}

/* Split out of CameraBasisFrom so the ECEF path reuses the EXACT same silent-mirror-critical logic
 * instead of a second copy that could drift from it. */
inline void CameraAxes(float yawDeg, float pitchDeg, float rollDeg, float f[3], float sr[3],
                         float up[3]) {
  const float RAD = (float)M_PI / 180.f;
  float yaw = yawDeg * RAD, pitch = pitchDeg * RAD, roll = rollDeg * RAD;

  /* Yaw 0 = north = -Z; yaw 90 = east = +X. */
  f[0] = cosf(pitch) * sinf(yaw);
  f[1] = sinf(pitch);
  f[2] = -cosf(pitch) * cosf(yaw);

  /* Degenerate with the nose straight up or down: the cross product vanishes and there IS no
   * screen-right. Vec3Normalize leaves a sub-epsilon vector alone, so the frame degrades to
   * garbage-but-finite rather than painting the screen with NaN. */
  float wup[3] = {0, 1, 0}, s[3], u[3];
  Vec3Cross(s, f, wup);
  Vec3Normalize(s);
  Vec3Cross(u, s, f);

  /* THE SILENT MIRROR: +roll = right bank must tilt camera-up toward the RIGHT (+s). This was once
   * -s, and a right bank looked like a left bank — nothing crashed. Pinned in test_camera.c. */
  for (int i = 0; i < 3; i++) {
    up[i] = u[i] * cosf(roll) + s[i] * sinf(roll);
    sr[i] = s[i] * cosf(roll) - u[i] * sinf(roll);
  }
}

inline CameraBasis CameraBasisFrom(float yawDeg, float pitchDeg, float rollDeg, const float eye[3],
                                       float fovDeg, float aspect, float znear, float zfar) {
  const float RAD = (float)M_PI / 180.f;
  CameraBasis c;
  CameraAxes(yawDeg, pitchDeg, rollDeg, c.f, c.sr, c.up);

  float ctr[3] = {eye[0] + c.f[0], eye[1] + c.f[1], eye[2] + c.f[2]};
  float view[16], proj[16];
  Mat4LookAt(view, eye, ctr, c.up);
  Mat4Perspective(proj, fovDeg * RAD, aspect, znear, zfar);
  Mat4Mul(c.mvp, proj, view);
  return c;
}

/* CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL
 * render-ENU, so they keep the flat-earth camera — the horizon dip emerges because the ECEF terrain
 * curves away below that local horizon. The rotation into ECEF axes uses core/Geodesy.h's
 * EnuAxesEcef (closed form, right-handed, det +1, so triangle winding survives) and is the OTHER
 * silent mirror: a wrongly tilted horizon still looks like a horizon. Pinned in test_camera.c. */

/* The ONE attitude->ECEF basis native and WASM share; render->ENU is (e,n,u) = (x,-z,y). Doubles,
 * not floats: the eye is an ECEF position in the 6.4e6 m range. */
inline void CameraBasisEcef(double yawDeg, double pitchDeg, double rollDeg, double latDeg,
                              double lonDeg, double fwd[3], double right[3], double up[3]) {
  double yaw = yawDeg * kDeg2Rad, pitch = pitchDeg * kDeg2Rad, roll = rollDeg * kDeg2Rad;
  double fR[3] = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
  double wup[3] = {0, 1, 0}, s[3], u[3];
  s[0] = fR[1] * wup[2] - fR[2] * wup[1];
  s[1] = fR[2] * wup[0] - fR[0] * wup[2];
  s[2] = fR[0] * wup[1] - fR[1] * wup[0];
  { double l = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    /* STRAIGHT DOWN AND STRAIGHT UP ARE NOT UNDEFINED, they are the limit: forward is parallel to
     * world up, so the cross product vanishes and dividing by a substituted 1.0 leaves a ZERO side
     * vector — from there `up` is zero too and the whole basis, and with it the projection, is
     * degenerate. The limit of (fwd x up) as pitch -> +-90 is the yaw's own horizontal right. */
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

/* translate touches only the last column, so columns 0..2 pass through — no full 4x4 multiply. */
inline void MvpTranslate(float out[16], const float vp[16], const float t[3]) {
  for (int i = 0; i < 12; i++)
    out[i] = vp[i];
  for (int r = 0; r < 4; r++)
    out[12 + r] = vp[r] * t[0] + vp[4 + r] * t[1] + vp[8 + r] * t[2] + vp[12 + r];
}

/* Gribb-Hartmann: inside iff every plane[k]·(x,y,z,1) >= 0. Same silent-mirror class — a wrong sign
 * culls on-screen terrain at one heading and no screenshot shows it. Planes stay UNnormalised: a sign
 * test needs no unit length. */
struct Frustum {
  float p[6][4];
}; /* mvp is column-major: mvp[col*4+row] */

inline Frustum FrustumFrom(const float mvp[16]) {
  Frustum fr;
  for (int a = 0; a < 3; a++) /* a: 0 L/R, 1 B/T, 2 N/F -- clip row a and row w */
    for (int j = 0; j < 4; j++) {
      float w = mvp[j * 4 + 3], c = mvp[j * 4 + a];
      fr.p[a * 2][j] = w + c;
      fr.p[a * 2 + 1][j] = w - c;
    }
  return fr;
}

/* Positive-vertex test, erring toward KEEPING: a false positive is one wasted draw, a false negative
 * would be a hole. */
inline int AabbVisible(const Frustum *fr, const float bmin[3], const float bmax[3]) {
  for (int k = 0; k < 6; k++) {
    const float *pl = fr->p[k];
    float d = pl[3] + pl[0] * (pl[0] >= 0 ? bmax[0] : bmin[0]) +
              pl[1] * (pl[1] >= 0 ? bmax[1] : bmin[1]) + pl[2] * (pl[2] >= 0 ? bmax[2] : bmin[2]);
    if (d < 0) return 0;
  }
  return 1;
}

} // namespace outshine
#endif /* CAMERA_H */
