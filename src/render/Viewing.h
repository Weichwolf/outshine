#ifndef OUTSHINE_RENDER_VIEWING_H
#define OUTSHINE_RENDER_VIEWING_H

#include <cmath>
#include <cstdint>

namespace outshine::Render {

// WHERE THE PICTURE IS SEEN FROM, and it belongs to the renderer rather than to a file format.
// The importer's own viewpoint type stood here: the render tier held a type from the glTF reader
// and used it as a
// plain data carrier -- no method of it was ever called from `src/render/`. A camera is not a glTF
// concept, it is the thing every renderer has; Filament spells it `Camera` at its door and Unreal's
// FSceneView is its own type with the importer nowhere near it.
//
// THE IMPORTER KEEPS ITS OWN, and that is not a duplicate. A glTF camera NODE is a real glTF thing
// and the importer's viewpoint reads it -- `Subject::Frame` and `DeclaredPlacement` both produce
// one. What
// changes is that the conversion happens ONCE, where a file's camera enters, instead of the whole
// render tier speaking the file's dialect.
enum class CameraKind : uint8_t { Perspective, Orthographic };

struct Viewpoint {
  double EyeM[3] = {0, 0, 0};
  double Forward[3] = {0, 0, -1};
  double Right[3] = {1, 0, 0};
  double Up[3] = {0, 1, 0};

  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  // AIMED FROM A PLACE AT A PLACE. Fifteen lines of cross products that belong to a camera, and the
  // engine called them through the importer's copy for want of its own.
  [[nodiscard]] static bool LookAt(const double eyeM[3], const double aimM[3], double rollRad,
                                   Viewpoint &out);
  [[nodiscard]] static bool LookAt(const double eyeM[3], const double aimM[3], const double upM[3],
                                   Viewpoint &out);
};

// INLINE, SO A LIGHT SUITE STAYS LIGHT. The importer needs this the moment its camera IS the
// renderer's, and three suites that link the importer without the render tier could not resolve it.
// Fifteen lines of cross products are not worth a link edge -- the alternative was making every one
// of those suites carry the renderer to place a camera.
namespace Aiming {

inline bool Normalise(double v[3]) {
  const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(len > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= len; }
  return true;
}

inline void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

}

inline bool Viewpoint::LookAt(const double eyeM[3], const double aimM[3], double rollRad,
                              Viewpoint &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Aiming::Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Aiming::Cross(forward, worldUp, right);
  if (!Aiming::Normalise(right)) { return false; }
  double up[3];
  Aiming::Cross(right, forward, up);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return true;
}

// AN UP VECTOR, WHICH IS WHAT FILAMENT'S `Camera::lookAt(eye, center, up)` TAKES. A roll angle
// needs a convention -- which way is positive, measured from what -- and this file's was written
// nowhere: a client holding the camera's own up had to recover an angle from it and guess the
// sense. Measured on Khronos's Triangle, whose camera rolls -26.57 degrees: the guess came out
// negated and the drawn facet shared 48% of its pixels with the oracle's. With the up vector
// handed over as it stands, the two agree exactly.
inline bool Viewpoint::LookAt(const double eyeM[3], const double aimM[3], const double upM[3],
                              Viewpoint &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Aiming::Normalise(forward)) { return false; }
  double right[3];
  Aiming::Cross(forward, upM, right);
  if (!Aiming::Normalise(right)) { return false; }
  double up[3];
  Aiming::Cross(right, forward, up);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  return true;
}

}
#endif
