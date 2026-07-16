/* The camera basis of ONE frame, as pure mathematics. No GL, no globals, no side effects.
 *
 * Attitude in, orthonormal basis + MVP out. Nothing else.
 *
 * This is the code that mirrors silently when it is wrong. The comment it replaces says so in
 * plain words: "The previous -s inverted it: a right bank looked like a left bank." Nothing
 * crashed, nothing looked broken — the world simply rolled the wrong way, and a screenshot cannot
 * tell you, because a banked horizon looks like a banked horizon either way. That class of bug is
 * exactly why mat4.h has tests, and this is the layer above it that had none.
 *
 * fov / aspect / near / far are PARAMETERS, not the renderer's constants. Same reason chunkmesh
 * takes `grid`: a module that knows the renderer's configuration cannot be tested against anything
 * but the renderer's configuration. W3_FOV stays where it belongs — at the call site.
 *
 * The eye position is NOT computed here on purpose. It needs `w3_yoff` (the osmmesh ground) and
 * `w3_nD`, which belong to the tile side; taking them would drag this file into an ownership
 * question it does not have. Position in, basis out.
 *
 * Render space is ENU: E=+X, up=+Y, N=-Z. Angles in degrees, as they arrive on the wire.
 */
#ifndef W3_CAMERA_H
#define W3_CAMERA_H

#include <math.h>
#include "gfx/mat4.h"

typedef struct {
    float f[3];      /* forward: where the nose points */
    float sr[3];     /* screen-right, AFTER roll */
    float up[3];     /* screen-up, AFTER roll */
    float mvp[16];   /* projection * view, column-major, ready for glUniformMatrix4fv */
} w3_cam;

static w3_cam w3_cam_from(float yaw_deg, float pitch_deg, float roll_deg,
                          const float eye[3], float fov_deg, float aspect,
                          float znear, float zfar)
{
    const float RAD = (float)M_PI / 180.f;
    float yaw = yaw_deg * RAD, pitch = pitch_deg * RAD, roll = roll_deg * RAD;
    w3_cam c;

    /* Forward from yaw/pitch. Yaw 0 = north = -Z; yaw 90 = east = +X. */
    c.f[0] =  cosf(pitch) * sinf(yaw);
    c.f[1] =  sinf(pitch);
    c.f[2] = -cosf(pitch) * cosf(yaw);

    /* Unrolled basis: screen-right is forward x world-up, screen-up completes it.
     * Degenerate when the nose points straight up or down (f parallel to world-up): the cross
     * product vanishes and there IS no defined screen-right — heading stops meaning anything.
     * v_norm leaves a below-epsilon vector alone rather than exploding it into NaN, so the frame
     * degrades to garbage-but-finite instead of painting the screen with NaN. No aircraft this
     * simulates flies there; the guarantee is only that it cannot poison the whole frame. */
    float wup[3] = {0, 1, 0}, s[3], u[3];
    v_cross(s, c.f, wup); v_norm(s);
    v_cross(u, s, c.f);

    /* Roll the basis around the forward axis. +roll = right bank = right wing down, and it must
     * tilt the camera-up toward the RIGHT (+s), so the world appears to roll LEFT in view. The
     * sign here was once -s, and a right bank looked like a left bank. See test_camera.c. */
    for (int i = 0; i < 3; i++) {
        c.up[i] = u[i] * cosf(roll) + s[i] * sinf(roll);
        c.sr[i] = s[i] * cosf(roll) - u[i] * sinf(roll);
    }

    float ctr[3] = {eye[0] + c.f[0], eye[1] + c.f[1], eye[2] + c.f[2]};
    float view[16], proj[16];
    m_lookat(view, eye, ctr, c.up);
    m_persp(proj, fov_deg * RAD, aspect, znear, zfar);
    m_mul(c.mvp, proj, view);
    return c;
}

/* Six inward half-spaces from an MVP (Gribb-Hartmann): inside iff every plane[k]·(x,y,z,1) >= 0.
 * Same silent-mirror class as the basis above -- a wrong sign culls on-screen terrain (a hole at
 * one heading) or nothing, neither crashes, no screenshot shows it. Pinned in test_camera.c.
 * Planes stay UNnormalised: a sign test does not need unit length, and it saves the sqrt. */
typedef struct { float p[6][4]; } w3_frustum;   /* mvp is column-major: mvp[col*4+row] */

static w3_frustum w3_frustum_from(const float mvp[16]) {
    w3_frustum fr;
    for (int a = 0; a < 3; a++)                  /* a: 0 L/R, 1 B/T, 2 N/F -- clip row a and row w */
        for (int j = 0; j < 4; j++) {
            float w = mvp[j*4+3], c = mvp[j*4+a];
            fr.p[a*2][j] = w + c; fr.p[a*2+1][j] = w - c;
        }
    return fr;
}

/* AABB at least partially inside? Positive-vertex test. Errs toward keeping (a false positive is one
 * wasted draw; a false negative would be a hole) -- the safe asymmetry for culling. */
static int w3_aabb_visible(const w3_frustum *fr, const float bmin[3], const float bmax[3]) {
    for (int k = 0; k < 6; k++) {
        const float *pl = fr->p[k];
        float d = pl[3]
            + pl[0] * (pl[0] >= 0 ? bmax[0] : bmin[0])
            + pl[1] * (pl[1] >= 0 ? bmax[1] : bmin[1])
            + pl[2] * (pl[2] >= 0 ? bmax[2] : bmin[2]);
        if (d < 0) return 0;
    }
    return 1;
}

#endif /* W3_CAMERA_H */
