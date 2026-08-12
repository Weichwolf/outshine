/* The view frustum of the camera-relative MVP, as planes, for the one job a stage has before it
 * records a draw: is this sphere in the picture at all.
 *
 * HEADER-ONLY and matrix-derived rather than angle-derived, because the projection carries an
 * off-centre principal point (the cockpit grid's `shift`) that a fov/aspect reconstruction would
 * silently drop — a plane read out of the matrix is right for whatever matrix the frame was built
 * with. Gribb/Hartmann, "Fast Extraction of Viewing Frustum Planes from the WorldViewProjection
 * Matrix". The eye sits at the origin in this space, so a test is one dot product per plane. */
#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace outshine::Render {

class Frustum {
public:
  /* `m` is column-major, the form FrameContext::Mvp20 carries. */
  void Set(const float m[16]) {
    N = 0;
    const double r0[4] = {m[0], m[4], m[8],  m[12]};
    const double r1[4] = {m[1], m[5], m[9],  m[13]};
    const double r2[4] = {m[2], m[6], m[10], m[14]};
    const double r3[4] = {m[3], m[7], m[11], m[15]};
    Add(r3, r0, +1.0);   /* left   */
    Add(r3, r0, -1.0);   /* right  */
    Add(r3, r1, +1.0);   /* bottom */
    Add(r3, r1, -1.0);   /* top    */
    /* w - z >= 0 is the NEAR plane under reversed Z, where z_clip is the constant zn and the plane
     * that would fall out of `row2` alone is degenerate. The far plane does not exist: the
     * projection is infinite. */
    Add(r3, r2, -1.0);
  }

  [[nodiscard]] bool Visible(const double c[3], double r) const {
    for (int i = 0; i < N; i++)
      if (P[i][0] * c[0] + P[i][1] * c[1] + P[i][2] * c[2] + P[i][3] < -r) return false;
    return true;
  }

  [[nodiscard]] bool Visible(const double origin[3], const float ctr[3], float rad) const {
    const double c[3] = {origin[0] + ctr[0], origin[1] + ctr[1], origin[2] + ctr[2]};
    return Visible(c, (double)rad);
  }

private:
  void Add(const double a[4], const double b[4], double s) {
    double p[4];
    for (int i = 0; i < 4; i++) p[i] = a[i] + s * b[i];
    const double len = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (!(len > 1.0e-12)) return;
    for (int i = 0; i < 4; i++) P[N][i] = p[i] / len;
    N++;
  }

  int N = 0;
  double P[5][4] = {};
};

} // namespace outshine::Render
#endif
