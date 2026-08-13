/* ONE ANIMATION CHANNEL'S DECODED CURVE, and it is `core/Keyframes.h` plus the ONE rule that
 * evaluator cannot carry: a rotation is interpolated on the SPHERE.
 *
 * WHY THAT ONE RULE LIVES HERE. `outshine::Keyframes` is arithmetic over number sequences and knows
 * none of its consumers -- a camera azimuth, a door hinge and an OSM way are the same object to it,
 * and none of them is a quaternion. "These four numbers are a rotation" is a statement the FILE
 * makes, through `target.path`, so the layer that reads the file is where it belongs. Everything
 * else -- the segment search, the clamp at both ends, glTF's Hermite basis with its tangents scaled
 * by the span -- is core's and is not restated.
 *
 * `LINEAR` OVER A QUATERNION IS SPHERICAL AND THE DIFFERENCE IS NOT SMALL: the component blend and
 * the spherical one agree at the keyframes and at the exact midpoint of a span, and nowhere else. A
 * quarter of the way through a 90-degree span they are 0.69 degrees apart, which reads as a rotation
 * that hurries through the middle of every span (`Specification.adoc`, Animations).
 *
 * A VIEW, NOT AN OWNER, the same as the evaluator it wraps: the two decoded runs are the caller's
 * and outlive it. A per-frame evaluation then costs no decode and no allocation. */
#ifndef GLTF_TRACK_H
#define GLTF_TRACK_H

#include <cstddef>
#include <vector>

#include "Types.h"

namespace outshine::Gltf {

class Track {
public:
  Track() = default;

  /* `times` is the sampler's input accessor decoded, one number per keyframe; `values` is its output
   * accessor, one element per keyframe or THREE under `CubicSpline`. The path supplies both the
   * component count and whether the blend is spherical, so the two cannot be set to disagree.
   *
   * REFUSES rather than half-building: an empty grid, a values run that is not the grid's length
   * times its components, a `CubicSpline` over one keyframe. `Weights` has as many components as the
   * mesh has morph targets, which the path cannot answer, so there it is divided out of the run. */
  [[nodiscard]] static bool Build(AnimationPath path, Interpolation how,
                                  const std::vector<double> &times,
                                  const std::vector<double> &values, Track &out);

  [[nodiscard]] bool Valid() const { return Curve_.Valid(); }
  size_t Components() const { return Curve_.Components(); }
  size_t KeyframeCount() const { return Curve_.Count(); }

  /* Writes `Components()` numbers. `seconds` is DERIVED from an integer frame index and never
   * accumulated (doc/requirements.md I.26.3); outside the grid the first or last value stands, which
   * is the format's clamp and not an extrapolation.
   *
   * A ROTATION COMES BACK ON THE UNIT SPHERE. Both the spherical blend and the spline leave a
   * quaternion off it, and `Transform::FromTrs` states from its own side what a non-unit quaternion
   * costs: a scale the node never declared. */
  void At(double seconds, double *out) const;

private:
  outshine::Keyframes Curve_;
  bool Spherical_ = false;
};

} // namespace outshine::Gltf
#endif
