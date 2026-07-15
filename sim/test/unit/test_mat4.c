/* Unit tests — command_center/gfx/mat4.h
 *
 * Matrix bugs do not crash. A flipped sign or a transposed layout renders a world that is merely
 * *wrong* — mirrored, inside out, or looking backwards — and the only oracle used to be a human
 * looking at pixels. These assert the properties that actually break: column-major layout (what
 * glUniformMatrix4fv with transpose=GL_FALSE demands), the -z view direction, and the handedness
 * of the camera basis.
 */
#include "tassert.h"
#include "../../command_center/gfx/mat4.h"

/* Apply a column-major m to a column vector v (w=1 in, w out). */
static void mv(const float *m, const float *v, float w, float *o){
    for(int r = 0; r < 4; r++)
        o[r] = m[0*4+r]*v[0] + m[1*4+r]*v[1] + m[2*4+r]*v[2] + m[3*4+r]*w;
}

void test_mat4(void){
    tsection("mat4: identity + multiply");
    {
        float I[16]; m_identity(I);
        ck(I[0]==1 && I[5]==1 && I[10]==1 && I[15]==1, "identity has a unit diagonal");
        float off = 0; for(int i = 0; i < 16; i++) if(i%5) off += I[i]*I[i];
        ck(off == 0.0f, "identity is zero off-diagonal");

        float v[3] = {3, -7, 2}, o[4];
        mv(I, v, 1, o);
        ck(o[0]==3 && o[1]==-7 && o[2]==2 && o[3]==1, "identity leaves a vector alone");

        /* A*I == A, for a deliberately asymmetric A: catches a transpose that symmetry hides. */
        float A[16]; for(int i = 0; i < 16; i++) A[i] = (float)(i*i % 7) - 3.0f;
        float R[16]; m_mul(R, A, I);
        int same = 1; for(int i = 0; i < 16; i++) if(R[i] != A[i]) same = 0;
        ck(same, "A*I == A (asymmetric A: a transpose bug cannot hide)");
        m_mul(R, I, A);
        same = 1; for(int i = 0; i < 16; i++) if(R[i] != A[i]) same = 0;
        ck(same, "I*A == A");

        /* m_mul must tolerate its output aliasing an input (it copies via a temp). */
        float B[16]; for(int i = 0; i < 16; i++) B[i] = A[i];
        m_mul(B, B, I);
        same = 1; for(int i = 0; i < 16; i++) if(B[i] != A[i]) same = 0;
        ck(same, "m_mul(o,o,b) aliasing is safe");

        /* Composition order: m_mul(o,a,b) must mean 'apply b, then a'. Translate-then-scale
         * differs from scale-then-translate, so this pins the order rather than assuming it. */
        float T[16]; m_identity(T); T[12] = 10;         /* translate +10 x */
        float S[16]; m_identity(S); S[0] = 2;           /* scale x by 2 */
        float TS[16]; m_mul(TS, T, S);                  /* = T*S : scale first, then translate */
        float p[3] = {1, 0, 0}, q[4]; mv(TS, p, 1, q);
        ck_near(q[0], 12.0f, 1e-5, "m_mul(o,T,S) applies S first, then T (x=1 -> 2 -> 12)");
    }

    tsection("mat4: vector helpers");
    {
        float v[3] = {3, 4, 0}; v_norm(v);
        ck_near(v[0], 0.6f, 1e-5, "v_norm scales to unit length (x)");
        ck_near(v[1], 0.8f, 1e-5, "v_norm scales to unit length (y)");

        /* a zero vector must survive rather than produce NaN */
        float z[3] = {0, 0, 0}; v_norm(z);
        ck(z[0]==0 && z[1]==0 && z[2]==0, "v_norm leaves a zero vector alone (no NaN)");
        float t[3] = {1e-9f, 0, 0}; v_norm(t);
        ck(t[0] == 1e-9f, "v_norm ignores a below-epsilon vector rather than exploding it");

        /* right-handed cross product: x cross y = +z */
        float a[3] = {1,0,0}, b[3] = {0,1,0}, c[3];
        v_cross(c, a, b);
        ck(c[0]==0 && c[1]==0 && c[2]==1, "v_cross is right-handed (x X y = +z)");
        v_cross(c, b, a);
        ck(c[2] == -1, "v_cross anticommutes (y X x = -z)");
    }

    tsection("mat4: perspective");
    {
        float P[16]; m_persp(P, 1.5707963f /*90deg*/, 1.0f, 1.0f, 100.0f);
        ck_near(P[0], 1.0f, 1e-4, "90deg fovy at aspect 1 -> m[0] = 1");
        ck_near(P[5], 1.0f, 1e-4, "90deg fovy -> m[5] = 1");
        ck(P[11] == -1.0f, "m[11] = -1: w_clip = -z_view, i.e. we look down -z");
        ck(P[15] == 0.0f, "perspective (not orthographic): m[15] = 0");

        /* the near plane maps to ndc z = -1, the far plane to +1 */
        float pn[3] = {0, 0, -1.0f}, on[4]; mv(P, pn, 1, on);
        ck_near(on[2]/on[3], -1.0f, 1e-4, "near plane -> ndc z = -1");
        float pf[3] = {0, 0, -100.0f}, of[4]; mv(P, pf, 1, of);
        ck_near(of[2]/of[3], 1.0f, 1e-4, "far plane -> ndc z = +1");

        /* wider aspect must squeeze x, or the world stretches on a wide window */
        float W[16]; m_persp(W, 1.5707963f, 2.0f, 1.0f, 100.0f);
        ck_near(W[0], 0.5f, 1e-4, "aspect 2 halves the x scale (no stretch on a wide window)");
        ck(W[5] == P[5], "aspect does not touch the y scale");

        /* a point right of centre must land right of centre (no mirror) */
        float pr[3] = {5, 0, -10}, orr[4]; mv(P, pr, 1, orr);
        ck(orr[0]/orr[3] > 0, "a point to the +x side stays on the +x side of the screen");
    }

    tsection("mat4: lookat");
    {
        /* camera at +z origin-ward, looking at the origin, y up */
        float eye[3] = {0, 0, 10}, ctr[3] = {0, 0, 0}, up[3] = {0, 1, 0};
        float V[16]; m_lookat(V, eye, ctr, up);

        float o[4];
        mv(V, ctr, 1, o);
        ck_near(o[2], -10.0f, 1e-4, "the look-at target sits 10 in FRONT of the camera (-z)");
        mv(V, eye, 1, o);
        ck_near(o[0], 0.0f, 1e-4, "the eye maps to the view origin (x)");
        ck_near(o[1], 0.0f, 1e-4, "the eye maps to the view origin (y)");
        ck_near(o[2], 0.0f, 1e-4, "the eye maps to the view origin (z)");

        /* world +y (up) must stay view +y; world +x must stay view +x for this pose */
        float wu[3] = {0, 5, 10}; mv(V, wu, 1, o);
        ck(o[1] > 0, "something above the camera renders above centre");
        float wr[3] = {5, 0, 10}; mv(V, wr, 1, o);
        ck(o[0] > 0, "something to the world +x renders right of centre (no mirror)");

        /* behind the camera must be behind: +z in view space */
        float wb[3] = {0, 0, 20}; mv(V, wb, 1, o);
        ck(o[2] > 0, "a point behind the camera lands behind it (+z view)");

        /* the basis must stay orthonormal — that is what makes it a rigid camera */
        float r[3] = {V[0], V[4], V[8]}, u2[3] = {V[1], V[5], V[9]}, f2[3] = {V[2], V[6], V[10]};
        ck_near(r[0]*r[0]+r[1]*r[1]+r[2]*r[2], 1.0f, 1e-4, "right axis is unit length");
        ck_near(u2[0]*u2[0]+u2[1]*u2[1]+u2[2]*u2[2], 1.0f, 1e-4, "up axis is unit length");
        ck_near(f2[0]*f2[0]+f2[1]*f2[1]+f2[2]*f2[2], 1.0f, 1e-4, "forward axis is unit length");
        ck_near(r[0]*u2[0]+r[1]*u2[1]+r[2]*u2[2], 0.0f, 1e-4, "right and up are perpendicular");
        ck_near(r[0]*f2[0]+r[1]*f2[1]+r[2]*f2[2], 0.0f, 1e-4, "right and forward are perpendicular");

        /* an oblique pose must stay orthonormal too (the y-up degenerate case is the easy one) */
        float e2[3] = {30, 12, -7}, c2[3] = {1, 2, 3};
        float V2[16]; m_lookat(V2, e2, c2, up);
        float r2[3] = {V2[0], V2[4], V2[8]};
        ck_near(r2[0]*r2[0]+r2[1]*r2[1]+r2[2]*r2[2], 1.0f, 1e-4, "oblique pose: basis stays unit");
        mv(V2, c2, 1, o);
        ck(o[2] < 0, "oblique pose: the target is still in front of the camera");
        ck_near(o[0], 0.0f, 1e-4, "oblique pose: the target is centred in x");
        ck_near(o[1], 0.0f, 1e-4, "oblique pose: the target is centred in y");
    }
}
