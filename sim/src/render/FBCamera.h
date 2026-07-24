/* The camera basis of ONE frame, as pure mathematics. No GL, no globals, no side effects.
 *
 * Attitude in, orthonormal basis + MVP out. Nothing else.
 *
 * This is the code that mirrors silently when it is wrong. The comment it replaces says so in
 * plain words: "The previous -s inverted it: a right bank looked like a left bank." Nothing
 * crashed, nothing looked broken — the world simply rolled the wrong way, and a screenshot cannot
 * tell you, because a banked horizon looks like a banked horizon either way. That class of bug is
 * exactly why FBMat4.h has tests, and this is the layer above it that had none.
 *
 * fov / aspect / near / far are PARAMETERS, not the renderer's constants. Same reason chunkmesh
 * takes `grid`: a module that knows the renderer's configuration cannot be tested against anything
 * but the renderer's configuration. W3_FOV stays where it belongs — at the call site.
 *
 * The eye position is NOT computed here on purpose. It needs `w3_O.yoff` (the osmmesh ground) and
 * `w3_frame.nD`, which belong to the tile side; taking them would drag this file into an ownership
 * question it does not have. Position in, basis out.
 *
 * Render space is ENU: E=+X, up=+Y, N=-Z. Angles in degrees, as they arrive on the wire.
 */
#ifndef FBCAMERA_H
#define FBCAMERA_H

#include "FBMat4.h"
#include <math.h>

typedef struct {
  float f[3];    /* forward: where the nose points */
  float sr[3];   /* screen-right, AFTER roll */
  float up[3];   /* screen-up, AFTER roll */
  float mvp[16]; /* projection * view, column-major, ready for glUniformMatrix4fv */
} w3_cam;

/* Earth mean radius (sphere), metres. The ECEF terrain uses the WGS84 ellipsoid; the horizon dip is
 * a small-angle spherical result for which the mean radius is the standard value. */
#define W3_EARTH_RADIUS_M 6371000.0f

/* Horizon dip: from height h above the local surface the visible horizon of a sphere of radius R
 * sinks acos(R/(R+h)) below the gravity-level horizontal (~sqrt(2h/R) for small h). A level HUD
 * horizon and a level sky gradient both read too HIGH aloft — they must drop by THIS angle to
 * overlay the real, curved-away horizon (MIL-STD-1787 conformal). h<=0 -> 0. */
static float w3_horizon_dip_rad(float alt_m) {
  if (alt_m <= 0.f) return 0.f;
  return acosf(W3_EARTH_RADIUS_M / (W3_EARTH_RADIUS_M + alt_m));
}

/* The orthonormal camera basis from an attitude, in RENDER space (E=+X, up=+Y, N=-Z). Split out of
 * w3_cam_from so the ECEF path below reuses the EXACT same silent-mirror-critical logic (forward
 * from yaw/pitch, the +s roll sign) instead of a second copy that could drift from it. */
static void w3_basis_from(float yaw_deg, float pitch_deg, float roll_deg, float f[3], float sr[3],
                          float up[3]) {
  const float RAD = (float)M_PI / 180.f;
  float yaw = yaw_deg * RAD, pitch = pitch_deg * RAD, roll = roll_deg * RAD;

  /* Forward from yaw/pitch. Yaw 0 = north = -Z; yaw 90 = east = +X. */
  f[0] = cosf(pitch) * sinf(yaw);
  f[1] = sinf(pitch);
  f[2] = -cosf(pitch) * cosf(yaw);

  /* Unrolled basis: screen-right is forward x world-up, screen-up completes it.
   * Degenerate when the nose points straight up or down (f parallel to world-up): the cross
   * product vanishes and there IS no defined screen-right — heading stops meaning anything.
   * v_norm leaves a below-epsilon vector alone rather than exploding it into NaN, so the frame
   * degrades to garbage-but-finite instead of painting the screen with NaN. No aircraft this
   * simulates flies there; the guarantee is only that it cannot poison the whole frame. */
  float wup[3] = {0, 1, 0}, s[3], u[3];
  v_cross(s, f, wup);
  v_norm(s);
  v_cross(u, s, f);

  /* Roll the basis around the forward axis. +roll = right bank = right wing down, and it must
   * tilt the camera-up toward the RIGHT (+s), so the world appears to roll LEFT in view. The
   * sign here was once -s, and a right bank looked like a left bank. See test_camera.c. */
  for (int i = 0; i < 3; i++) {
    up[i] = u[i] * cosf(roll) + s[i] * sinf(roll);
    sr[i] = s[i] * cosf(roll) - u[i] * sinf(roll);
  }
}

static w3_cam w3_cam_from(float yaw_deg, float pitch_deg, float roll_deg, const float eye[3],
                          float fov_deg, float aspect, float znear, float zfar) {
  const float RAD = (float)M_PI / 180.f;
  w3_cam c;
  w3_basis_from(yaw_deg, pitch_deg, roll_deg, c.f, c.sr, c.up);

  float ctr[3] = {eye[0] + c.f[0], eye[1] + c.f[1], eye[2] + c.f[2]};
  float view[16], proj[16];
  m_lookat(view, eye, ctr, c.up);
  m_persp(proj, fov_deg * RAD, aspect, znear, zfar);
  m_mul(c.mvp, proj, view);
  return c;
}

/* ==== ECEF camera-relative rendering (the terrain camera) ======================================
 * Terrain is drawn on the WGS84 ellipsoid with the camera at the coordinate origin (floating
 * origin) — best precision AT the camera everywhere on earth, curvature exact, no dateline/pole
 * special cases. w3_cam_from above is NOT dead: the sky dome and star field are an infinity pass in
 * LOCAL render-ENU (up=+Y), so they keep the flat-earth camera; the geometric horizon dip emerges
 * because the ECEF terrain curves away below that local horizon.
 *
 * Attitude arrives in the aircraft's LOCAL ENU frame (north/east/up at its lat/lon). To view ECEF
 * geometry we rotate the local camera basis into ECEF axes by the ENU->ECEF rotation at the
 * camera's lat/lon. THIS is the new mathematics, and the one that mirrors silently if a sign is
 * wrong — a wrong-tilted horizon still looks like a horizon. Pinned in test_camera.c against the
 * closed-form ENU axes (Polaris-class checks: at (0,0) up is +X, east is +Y, north is +Z, etc.). */

/* ENU basis vectors expressed in ECEF at geodetic (lat,lon) in degrees. These are the columns of
 * the ENU->ECEF rotation:
 *   East  = (-sinL,        cosL,       0   )
 *   North = (-sinP cosL,  -sinP sinL,  cosP)
 *   Up    = ( cosP cosL,   cosP sinL,  sinP)
 * Closed form, right-handed, det +1 (a proper rotation) — so it preserves triangle winding. */
static void w3_enu_axes_ecef(double lat_deg, double lon_deg, double E[3], double N[3],
                             double U[3]) {
  const double RAD = M_PI / 180.0;
  double P = lat_deg * RAD, L = lon_deg * RAD;
  double sP = sin(P), cP = cos(P), sL = sin(L), cL = cos(L);
  E[0] = -sL;
  E[1] = cL;
  E[2] = 0.0;
  N[0] = -sP * cL;
  N[1] = -sP * sL;
  N[2] = cP;
  U[0] = cP * cL;
  U[1] = cP * sL;
  U[2] = sP;
}

/* Rotate a RENDER-space vector (E=+X, up=+Y, N=-Z) into ECEF axes at (lat,lon). The render->ENU
 * remap is (e,n,u) = (x, -z, y); then v_ecef = E*e + N*n + U*u. */
static void w3_render_to_ecef(double lat_deg, double lon_deg, const float rv[3], float out[3]) {
  double E[3], N[3], U[3];
  w3_enu_axes_ecef(lat_deg, lon_deg, E, N, U);
  double e = rv[0], u = rv[1], n = -rv[2];
  for (int i = 0; i < 3; i++)
    out[i] = (float)(E[i] * e + N[i] * n + U[i] * u);
}

/* Camera for the ECEF path: eye at the origin (0,0,0), the attitude basis rotated into ECEF axes.
 * c.mvp is the VIEW-PROJECTION only (proj * view(eye=0)); the per-tile model translation
 * (origin_ecef - cam_ecef, as float) is post-multiplied at draw time (w3_mvp_translate) so a tile's
 * small local offsets never mix with the large ECEF magnitudes. c.f/sr/up are the ECEF basis. */
static w3_cam w3_cam_ecef(float yaw_deg, float pitch_deg, float roll_deg, double cam_lat,
                          double cam_lon, float fov_deg, float aspect, float znear, float zfar) {
  const float RAD = (float)M_PI / 180.f;
  float fR[3], sR[3], uR[3]; /* render-space basis (local ENU orientation) */
  w3_basis_from(yaw_deg, pitch_deg, roll_deg, fR, sR, uR);

  w3_cam c;
  w3_render_to_ecef(cam_lat, cam_lon, fR, c.f);
  w3_render_to_ecef(cam_lat, cam_lon, sR, c.sr);
  w3_render_to_ecef(cam_lat, cam_lon, uR, c.up);

  const float eye[3] = {0, 0, 0};
  float ctr[3] = {c.f[0], c.f[1], c.f[2]};
  float view[16], proj[16];
  m_lookat(view, eye, ctr, c.up);
  m_persp(proj, fov_deg * RAD, aspect, znear, zfar);
  m_mul(c.mvp, proj, view);
  return c;
}

/* out = viewproj * translate(t). translate touches only the last column, so columns 0..2 pass
 * through and column 3 = VP*(t,1). Column-major (m[col*4+row]). Cheap: no full 4x4 multiply. */
static void w3_mvp_translate(float out[16], const float vp[16], const float t[3]) {
  for (int i = 0; i < 12; i++)
    out[i] = vp[i];
  for (int r = 0; r < 4; r++)
    out[12 + r] = vp[r] * t[0] + vp[4 + r] * t[1] + vp[8 + r] * t[2] + vp[12 + r];
}

/* Six inward half-spaces from an MVP (Gribb-Hartmann): inside iff every plane[k]·(x,y,z,1) >= 0.
 * Same silent-mirror class as the basis above -- a wrong sign culls on-screen terrain (a hole at
 * one heading) or nothing, neither crashes, no screenshot shows it. Pinned in test_camera.c.
 * Planes stay UNnormalised: a sign test does not need unit length, and it saves the sqrt. */
typedef struct {
  float p[6][4];
} w3_frustum; /* mvp is column-major: mvp[col*4+row] */

static w3_frustum w3_frustum_from(const float mvp[16]) {
  w3_frustum fr;
  for (int a = 0; a < 3; a++) /* a: 0 L/R, 1 B/T, 2 N/F -- clip row a and row w */
    for (int j = 0; j < 4; j++) {
      float w = mvp[j * 4 + 3], c = mvp[j * 4 + a];
      fr.p[a * 2][j] = w + c;
      fr.p[a * 2 + 1][j] = w - c;
    }
  return fr;
}

/* AABB at least partially inside? Positive-vertex test. Errs toward keeping (a false positive is
 * one wasted draw; a false negative would be a hole) -- the safe asymmetry for culling. */
static int w3_aabb_visible(const w3_frustum *fr, const float bmin[3], const float bmax[3]) {
  for (int k = 0; k < 6; k++) {
    const float *pl = fr->p[k];
    float d = pl[3] + pl[0] * (pl[0] >= 0 ? bmax[0] : bmin[0]) +
              pl[1] * (pl[1] >= 0 ? bmax[1] : bmin[1]) + pl[2] * (pl[2] >= 0 ? bmax[2] : bmin[2]);
    if (d < 0) return 0;
  }
  return 1;
}

#endif /* FBCAMERA_H */
