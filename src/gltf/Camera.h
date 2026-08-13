/* THE glTF CAMERA, AND ITS TWO CONVENTIONS STATED ONCE.
 *
 * HANDEDNESS: a camera sits in its node's local space looking down **-Z**, with **+Y up** and **+X
 * right**, right-handed. The view transform is therefore the INVERSE of the camera node's world
 * transform, and nothing here flips a sign to taste.
 *
 * CLIP SPACE: the format's projection puts NDC z in [-1, +1] with -1 at the near plane. That is not
 * this engine's depth convention (core/Mat4.h is reversed-Z), and converting between them is the
 * renderer's business at the point where it builds a pipeline -- the file's meaning stays the file's.
 *
 * `aspectRatio` and `zfar` are OPTIONAL in the format. `aspectRatio` is carried because it is what
 * the file says and never applied, because the frame the picture lands in is what decides the
 * horizontal extent; absent zfar means an infinite frustum, which is a different matrix and not a
 * large number substituted for one. */
#ifndef GLTF_CAMERA_H
#define GLTF_CAMERA_H

#include <string>

#include "Transform.h"
#include "Types.h"

namespace outshine::Gltf {

/* Where NDC lands in pixels. THE INTEGER RASTER COORDINATE IS THE PIXEL CENTRE, which is what makes
 * a centre-sampling rasteriser and Cycles at a hairline box filter evaluate the same predicate
 * (doc/requirements.md I.26); y runs downward, the row order the oracle's raw dump carries. */
struct Viewport {
  double WidthPx = 0;
  double HeightPx = 0;

  double Aspect() const { return (HeightPx > 0) ? WidthPx / HeightPx : 0.0; }
  void Raster(const double ndc[3], double outPx[2]) const {
    outPx[0] = (ndc[0] * 0.5 + 0.5) * WidthPx - 0.5;
    outPx[1] = (0.5 - ndc[1] * 0.5) * HeightPx - 0.5;
  }
};

struct Camera {
  std::string Name;
  CameraKind Kind = CameraKind::Perspective;

  double YfovRad = 0;
  /* 0 means the format did not state it, and the file's own number is not read by `Projection`
   * either -- see the head of this file. */
  double AspectRatio = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  double XMagM = 0;
  double YMagM = 0;

  /* Refuses rather than producing a matrix full of infinities: a zero yfov, a zero magnification or
   * a near plane at the eye are files that cannot mean anything, and a caller that gets `false` can
   * say which file it was. */
  [[nodiscard]] bool Projection(double viewportAspect, Transform &out) const;
};

} // namespace outshine::Gltf
#endif
