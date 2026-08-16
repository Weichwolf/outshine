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
