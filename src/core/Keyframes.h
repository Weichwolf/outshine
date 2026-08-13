#ifndef KEYFRAMES_H
#define KEYFRAMES_H

#include <cstddef>

namespace outshine {

/* THE KEYFRAME EVALUATOR, and it knows none of its consumers. A camera azimuth, a door hinge, an
 * entity on a path and a smoothed OSM way are the same object here: keyframes, values, an
 * interpolation. That is why it sits in core/ — arithmetic over number sequences, no I/O, no
 * knowledge of the declaration layer. READING a track out of JSON belongs to the declaration layer
 * (F.2) and GENERATING tangents from bare values is a second operation of its own
 * (core/CatmullRom.h); an evaluator with Catmull-Rom built in would be wrong for the one consumer
 * that brings its own tangents, and a Catmull-Rom with evaluation built in would be wrong for the
 * one that runs at bake time.
 *
 * A VIEW, not an owner: the arrays are the caller's and outlive it. N components with a stride
 * rather than a fixed value type, and the result is written into a caller-provided buffer — the
 * heaviest consumer smooths hundreds of thousands of OSM points at bake time, where a returned
 * vector would be one allocation per point and a virtual call would be one per point as well.
 *
 * THE ABSCISSA IS FRAMES for a scene channel and knot distance for a curve (Prinzip 7: a run must
 * deliver the same N frames every time, and a seconds axis couples the result to the wall clock).
 * Either way it only has to strictly increase.
 *
 * AN ANGLE TRACK CARRIES AN ACCUMULATED ANGLE, not a compass bearing, and it is never wrapped here.
 * 280 -> 640 is one full turn forward; 350 -> 10 sweeps backwards through the whole picture, and an
 * author who wanted +20 deg writes 350 -> 370. A shortest-arc rule would make the full turn
 * (delta 360 -> 0) impossible to state, so the reduction modulo 360 happens where the value is
 * APPLIED and nowhere else. */
class Keyframes {
public:
  enum class Interpolation {
    Step,          /* switching quantities: a weather state, a mode */
    Linear,        /* C0 only — the velocity jumps at every key, which a camera shows as a kink */
    CubicSpline,   /* C1 from glTF's triples: in-tangent, value, out-tangent per key, per component */
  };

  Keyframes() = default;
  /* `values` holds `components` numbers per keyframe, or three times that for CubicSpline, laid out
   * as glTF does it: all in-tangents, then all values, then all out-tangents, per keyframe. */
  Keyframes(Interpolation how, const double *frames, size_t count, const double *values,
            size_t components)
      : Frames_(frames), Values_(values), Count_(count), Components_(components), How_(how) {}

  [[nodiscard]] bool Valid() const { return Frames_ && Values_ && Count_ > 0 && Components_ > 0; }
  size_t Count() const { return Count_; }
  size_t Components() const { return Components_; }
  [[nodiscard]] Interpolation How() const { return How_; }

  /* Writes Components() numbers. Outside the keyframe range the first / last value stands, as in
   * glTF. */
  void At(double abscissa, double *out) const;

  /* WHICH SPAN AN ABSCISSA FALLS IN AND HOW FAR THROUGH IT. A consumer whose blend is none of the
   * three above — a quaternion, which glTF interpolates on the sphere — needs the same segment
   * search and the same weight, and writing a second one is how the two come to disagree about
   * where a span ends. `false` outside the range and on a single keyframe, which is exactly where
   * `At` clamps and there is no span to be in. */
  [[nodiscard]] bool Span(double abscissa, size_t &keyframe, double &weight) const;
  /* The value at one keyframe, past the tangents where the layout carries them. */
  const double *ValueAt(size_t keyframe) const { return Value(keyframe); }
  double AtScalar(double abscissa) const {
    double v = 0.0;
    At(abscissa, &v);
    return v;
  }

private:
  size_t Segment(double abscissa) const;
  const double *Value(size_t k) const {
    return Values_ + k * Components_ * (How_ == Interpolation::CubicSpline ? 3u : 1u) +
           (How_ == Interpolation::CubicSpline ? Components_ : 0u);
  }
  const double *OutTangent(size_t k) const { return Values_ + (k * 3u + 2u) * Components_; }
  const double *InTangent(size_t k) const { return Values_ + k * 3u * Components_; }

  const double *Frames_ = nullptr;
  const double *Values_ = nullptr;
  size_t Count_ = 0, Components_ = 0;
  Interpolation How_ = Interpolation::Linear;
};

} // namespace outshine
#endif
