/* THE CAMERA PATH EVERY FRAME MEASUREMENT IS TAKEN OVER, in one place because it is one statement.
 * A frame cost is a distribution and a still frame has none (CLAUDE.md), so every instrument in this
 * layer needs a moving eye -- and two instruments each carrying their own orbit would be two paths
 * whose numbers read as comparable and are not. */
#ifndef ORBIT_H
#define ORBIT_H

#include <cmath>

#include "Subject.h"

namespace outshine::Test {

/* WHERE THE EYE IS ON FRAME `step`: one turn around the subject, STARTED AT THE FRAMING THE SUBJECT
 * ITSELF DERIVED and turned about that placement's own up axis. Anchoring it to the framing is not a
 * detail -- an orbit started on a world axis put the camera edge-on to a flat subject at step zero,
 * and the arm that read its coverage there priced a ray over 1 277 pixels of a 921 600-pixel frame.
 *
 * `scale` MOVES THE WHOLE ORBIT IN OR OUT, and it is what lets one subject be measured both at the
 * distance it was built to be seen at and at the distance where it fills the frame. The frame-filling
 * distance is the one the question "does a shadow ray per screen pixel fit" is actually asked at. */
inline outshine::Gltf::Placement OrbitAt(const outshine::Gltf::Subject &subject,
                                         const outshine::Gltf::Placement &framed, double scale,
                                         int step, int steps) {
  double centre[3];
  subject.CentreM(centre);
  double radial[3];
  double distance = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    radial[axis] = framed.EyeM[axis] - centre[axis];
    distance += radial[axis] * radial[axis];
  }
  distance = std::sqrt(distance);
  for (int axis = 0; axis < 3; ++axis) { radial[axis] /= distance; }
  /* The third axis of the framed placement's own basis, so the turn is about the subject's up and
   * not about whichever world axis happened to be handy. */
  const double up[3] = {framed.Up[0], framed.Up[1], framed.Up[2]};
  const double side[3] = {up[1] * radial[2] - up[2] * radial[1],
                          up[2] * radial[0] - up[0] * radial[2],
                          up[0] * radial[1] - up[1] * radial[0]};
  const double turn = 2.0 * 3.14159265358979323846 * (double)step / (double)steps;
  const double tilt = 0.35 * std::sin(turn * 2.0);
  double eye[3];
  for (int axis = 0; axis < 3; ++axis) {
    eye[axis] = centre[axis] + distance * scale *
                                   (std::cos(tilt) * (std::cos(turn) * radial[axis] +
                                                      std::sin(turn) * side[axis]) +
                                    std::sin(tilt) * up[axis]);
  }
  outshine::Gltf::Placement placed = framed;
  /* THE DEPTH WINDOW FOLLOWS THE EYE (board:1436), and it is the same rule `board:1433` took for the far plane one
   * suite over: **a clip range is a depth window and never a crop**, so a placement that moves the
   * camera to a quarter of its framing distance and leaves the near plane where the full distance put
   * it declares a subject in front of its own near plane. [MEASURED] `SciFiHelmet` at `scale` 0.25 has
   * its nearest vertex 4.065543 m along the view axis against a declared near plane of 13.758289 m, and
   * the studio refused the whole arm for it -- correctly, and about the placement rather than about the
   * subject. The window is re-derived here from the distance this orbit actually stands at and the
   * radius the subject actually has, which is the same expression the framing rule uses. */
  const double orbitDistance = distance * scale;
  double radius = 0.0;
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double away = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double off = subject.PositionsM()[vertex * 3 + (size_t)axis] - centre[axis];
      away += off * off;
    }
    radius = std::fmax(radius, std::sqrt(away));
  }
  /* `distance - radius` IS ALREADY THE BOUND AND THE HALF IS ONLY CLEARANCE. Every vertex lies within
   * `radius` of the centre, so none can be nearer than `distance - radius` along any axis -- but a
   * spherical subject ATTAINS that bound at one vertex, and the studio refuses a vertex sitting exactly
   * ON the plane, so the plane is put at half the clearance rather than at the bound itself. A first
   * reading of this clamped the bound to `distance * 0.5` for standpoints inside the bounding sphere and
   * [MEASURED] refused `normal-tangent` at 0.489680 m against a plane of 0.496809 -- **a margin invented
   * to cover a case, which then covered it wrongly**. Where the eye IS inside the sphere there is no
   * positive bound to be had, and zero hands the question to the renderer's own constant, which is the
   * same number `ClearsNearPlane` falls back to.
   *
   * THE FAR PLANE BOUNDS NOTHING THE ENGINE DRAWS -- its projection is reversed-Z and infinite -- so it
   * is set generously for the harness's own analytic clip and is not a picture decision. */
  const double clearance = orbitDistance - radius;
  placed.ZNearM = clearance > 0.0 ? clearance * 0.5 : 0.0;
  placed.ZFarM = (orbitDistance + radius) * 2.0;
  /* A PARALLEL PROJECTION HAS NO DISTANCE, so moving the eye in would change nothing: its extent is
   * the magnification, and that is what `scale` has to move instead. */
  if (placed.Kind == outshine::Gltf::CameraKind::Orthographic) {
    placed.XMagM *= scale;
    placed.YMagM *= scale;
  }
  (void)outshine::Gltf::Placement::LookAt(eye, centre, 0.0, placed);
  if (placed.Kind == outshine::Gltf::CameraKind::Orthographic) {
    placed.XMagM = framed.XMagM * scale;
    placed.YMagM = framed.YMagM * scale;
  }
  return placed;
}

} // namespace outshine::Test
#endif
