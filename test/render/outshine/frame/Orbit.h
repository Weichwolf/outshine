#ifndef ORBIT_H
#define ORBIT_H

#include <cmath>

#include "Subject.h"

namespace outshine::Test {

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

  const double clearance = orbitDistance - radius;
  placed.ZNearM = clearance > 0.0 ? clearance * 0.5 : 0.0;
  placed.ZFarM = (orbitDistance + radius) * 2.0;

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

}
#endif
