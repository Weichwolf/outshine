/* Unit tests — command_center/camera.h  (attitude -> basis + MVP)
 *
 * This is the silent-mirror layer. mat4.h has tests because a transposed matrix renders a world
 * that is merely WRONG — mirrored, inside out, backwards — and never crashes. camera.h sits one
 * storey up and had none, even though its own comment records a bug of exactly that kind:
 *
 *     "The previous -s inverted it: a right bank looked like a left bank."
 *
 * A screenshot cannot catch that. A banked horizon looks like a banked horizon whichever way it
 * leans, and the only oracle was a human noticing that the aircraft rolled the wrong way. So the
 * roll sign is pinned below at an angle where the answer is exact, not eyeballed.
 */
#include "tassert.h"
#include "../../command_center/camera.h"
#include <math.h>

/* Apply a column-major m to a column vector (w=1 in, w out) — same helper as test_mat4.c. */
static void mv(const float *m, const float *v, float w, float *o) {
    for (int r = 0; r < 4; r++)
        o[r] = m[0*4+r]*v[0] + m[1*4+r]*v[1] + m[2*4+r]*v[2] + m[3*4+r]*w;
}

void test_camera(void) {
    const float origin[3] = {0, 0, 0};

    tsection("camera: heading axes are ENU (N=-Z, E=+X)");
    {
        /* Facing north, level. The axis convention is the one that costs a whole flipped world
         * and says nothing while it does it. */
        w3_cam c = w3_cam_from(0, 0, 0, origin, 80, 1.333f, 20, 240000);
        ck_near(c.f[0],  0.0f, 1e-5, "yaw=0: forward x = 0");
        ck_near(c.f[1],  0.0f, 1e-5, "yaw=0, pitch=0: forward y = 0 (level)");
        ck_near(c.f[2], -1.0f, 1e-5, "yaw=0 (north) points to -Z, not +Z");

        c = w3_cam_from(90, 0, 0, origin, 80, 1.333f, 20, 240000);
        ck_near(c.f[0], 1.0f, 1e-5, "yaw=90 (east) points to +X");
        ck_near(c.f[2], 0.0f, 1e-5, "yaw=90: forward z = 0");

        c = w3_cam_from(0, 30, 0, origin, 80, 1.333f, 20, 240000);
        ck_near(c.f[1], 0.5f, 1e-5, "pitch=+30 -> nose UP (+Y), sin(30) = 0.5");
        ck(c.f[2] < 0, "pitch=+30 while facing north: still going north");

        c = w3_cam_from(0, -90, 0, origin, 80, 1.333f, 20, 240000);
        ck_near(c.f[1], -1.0f, 1e-5, "pitch=-90 -> straight down (-Y)");
    }

    tsection("camera: THE ROLL SIGN — a right bank must not look like a left one");
    {
        /* Facing north and level: forward=(0,0,-1) so screen-right s=(1,0,0)=EAST and
         * camera-up u=(0,1,0). At roll=+90 the whole basis has turned a quarter turn about the
         * nose, so up must land exactly ON s (pointing east/right) and screen-right on -u
         * (pointing down). Both answers are exact, so this cannot be argued with.
         *
         * With the old `-s` the up vector would land on (-1,0,0) — west — and the world would
         * roll the wrong way. That is the entire bug, and it is one sign. */
        w3_cam c = w3_cam_from(0, 0, 90, origin, 80, 1.333f, 20, 240000);
        ck_near(c.up[0],  1.0f, 1e-5, "roll=+90 (right bank): camera-up tilts toward +s = EAST");
        ck_near(c.up[1],  0.0f, 1e-5, "roll=+90: up y = 0");
        ck_near(c.sr[0],  0.0f, 1e-5, "roll=+90: screen-right x = 0");
        ck_near(c.sr[1], -1.0f, 1e-5, "roll=+90: screen-right has turned to point DOWN (-u)");

        /* A small right bank must move up toward +x, not -x. The 90-deg case pins the magnitude;
         * this pins the direction at a realistic angle, where a sign error is subtle. */
        c = w3_cam_from(0, 0, 15, origin, 80, 1.333f, 20, 240000);
        ck(c.up[0] > 0, "a 15-deg RIGHT bank tilts camera-up to the RIGHT (+x), not the left");
        ck(c.up[1] > 0.9f, "a 15-deg bank leaves up mostly up");

        c = w3_cam_from(0, 0, -15, origin, 80, 1.333f, 20, 240000);
        ck(c.up[0] < 0, "a 15-deg LEFT bank tilts camera-up to the LEFT — the mirror of the above");

        /* Level flight must not roll at all: catches a sign that only shows under bank. */
        c = w3_cam_from(0, 0, 0, origin, 80, 1.333f, 20, 240000);
        ck_near(c.up[0], 0.0f, 1e-5, "roll=0: no tilt at all");
        ck_near(c.up[1], 1.0f, 1e-5, "roll=0: up is up");
    }

    tsection("camera: the basis stays orthonormal — that is what makes it rigid");
    {
        /* An oblique attitude: the easy cases have a 0 or 1 in every slot and hide a missing
         * normalisation. */
        w3_cam c = w3_cam_from(37, 12, -23, origin, 80, 1.333f, 20, 240000);
        float lf = sqrtf(c.f[0]*c.f[0] + c.f[1]*c.f[1] + c.f[2]*c.f[2]);
        float lu = sqrtf(c.up[0]*c.up[0] + c.up[1]*c.up[1] + c.up[2]*c.up[2]);
        float ls = sqrtf(c.sr[0]*c.sr[0] + c.sr[1]*c.sr[1] + c.sr[2]*c.sr[2]);
        ck_near(lf, 1.0f, 1e-5, "oblique: forward is unit length");
        ck_near(lu, 1.0f, 1e-5, "oblique: up is unit length");
        ck_near(ls, 1.0f, 1e-5, "oblique: screen-right is unit length");

        float fu = c.f[0]*c.up[0] + c.f[1]*c.up[1] + c.f[2]*c.up[2];
        float fs = c.f[0]*c.sr[0] + c.f[1]*c.sr[1] + c.f[2]*c.sr[2];
        float us = c.up[0]*c.sr[0] + c.up[1]*c.sr[1] + c.up[2]*c.sr[2];
        ck_near(fu, 0.0f, 1e-5, "oblique: forward and up are perpendicular");
        ck_near(fs, 0.0f, 1e-5, "oblique: forward and screen-right are perpendicular");
        ck_near(us, 0.0f, 1e-5, "oblique: up and screen-right are perpendicular");
    }

    tsection("camera: the MVP puts the nose in the middle and does not mirror");
    {
        const float eye[3] = {100, 500, -200};
        w3_cam c = w3_cam_from(0, 0, 0, eye, 80, 1.333f, 20, 240000);

        float ahead[3] = {eye[0] + c.f[0]*1000, eye[1] + c.f[1]*1000, eye[2] + c.f[2]*1000};
        float o[4]; mv(c.mvp, ahead, 1, o);
        ck(o[3] > 0, "a point 1 km ahead is in front of the camera (w > 0)");
        ck_near(o[0]/o[3], 0.0f, 1e-4, "straight ahead lands at screen centre x");
        ck_near(o[1]/o[3], 0.0f, 1e-4, "straight ahead lands at screen centre y");

        /* Offset along the camera's own screen-right must land right of centre. This is the
         * check that fails when the basis is mirrored but still orthonormal. */
        float right[3] = {ahead[0] + c.sr[0]*100, ahead[1] + c.sr[1]*100, ahead[2] + c.sr[2]*100};
        mv(c.mvp, right, 1, o);
        ck(o[0]/o[3] > 0, "a point to the camera's screen-RIGHT renders right of centre");

        float above[3] = {ahead[0] + c.up[0]*100, ahead[1] + c.up[1]*100, ahead[2] + c.up[2]*100};
        mv(c.mvp, above, 1, o);
        ck(o[1]/o[3] > 0, "a point along camera-UP renders above centre");

        /* The eye itself must not survive the projection as a visible point. */
        mv(c.mvp, eye, 1, o);
        ck(o[3] <= 0.001f, "the eye position itself is not in front of the camera");
    }

    tsection("camera: nose straight up is degenerate — it must stay FINITE");
    {
        /* Forward parallel to world-up: there is no defined screen-right, the cross product
         * vanishes. No aircraft here flies there, but "cannot happen" is not a guard — and a NaN
         * in the MVP does not produce a wrong picture, it produces NO picture, silently. */
        w3_cam c = w3_cam_from(0, 90, 0, origin, 80, 1.333f, 20, 240000);
        int finite = 1;
        for (int i = 0; i < 16; i++) if (isnan(c.mvp[i]) || isinf(c.mvp[i])) finite = 0;
        for (int i = 0; i < 3; i++)
            if (isnan(c.f[i]) || isnan(c.up[i]) || isnan(c.sr[i])) finite = 0;
        ck(finite, "pitch=+90: basis and MVP are finite (garbage is survivable, NaN is not)");
        ck_near(c.f[1], 1.0f, 1e-5, "pitch=+90: forward still points straight up");
    }

    tsection("camera: it is a pure function");
    {
        const float eye[3] = {7, 8, 9};
        w3_cam a = w3_cam_from(31, -4, 12, eye, 80, 1.333f, 20, 240000);
        w3_cam b = w3_cam_from(31, -4, 12, eye, 80, 1.333f, 20, 240000);
        ck(memcmp(&a, &b, sizeof a) == 0, "same attitude -> byte-identical camera");

        /* fov/aspect/near/far are parameters, not the renderer's constants — so the test can
         * change them and see the projection follow. A module that knew W3_FOV could only ever
         * be tested at W3_FOV. */
        w3_cam wide = w3_cam_from(0, 0, 0, eye, 120, 1.333f, 20, 240000);
        w3_cam narrow = w3_cam_from(0, 0, 0, eye, 40, 1.333f, 20, 240000);
        ck(fabsf(wide.mvp[0]) < fabsf(narrow.mvp[0]), "a wider fov squeezes x (more world per pixel)");
        ck(memcmp(wide.f, narrow.f, sizeof wide.f) == 0, "fov does not touch the basis");
    }
}
