#include "Keyframes.h"

namespace outshine {

size_t Keyframes::Segment(double abscissa) const {
  size_t lo = 0;
  size_t hi = Count_ - 1;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    if (abscissa < Frames_[mid]) {
      hi = mid;
    } else {
      lo = mid;
    }
  }
  return lo;
}

bool Keyframes::Span(double abscissa, size_t &keyframe, double &weight) const {
  if (!Valid() || Count_ < 2) { return false; }
  if (abscissa <= Frames_[0] || abscissa >= Frames_[Count_ - 1]) { return false; }
  keyframe = Segment(abscissa);
  const double td = Frames_[keyframe + 1] - Frames_[keyframe];

  weight = (td > 0.0) ? (abscissa - Frames_[keyframe]) / td : 0.0;
  return true;
}

void Keyframes::At(double abscissa, double *out) const {
  if (!Valid()) { return; }
  if (Count_ == 1 || abscissa <= Frames_[0]) {
    const double *v = Value(0);
    for (size_t c = 0; c < Components_; c++) { out[c] = v[c]; }
    return;
  }
  if (abscissa >= Frames_[Count_ - 1]) {
    const double *v = Value(Count_ - 1);
    for (size_t c = 0; c < Components_; c++) { out[c] = v[c]; }
    return;
  }

  const size_t k = Segment(abscissa);
  const double td = Frames_[k + 1] - Frames_[k];
  const double t = (abscissa - Frames_[k]) / td;
  const double *a = Value(k);
  const double *b = Value(k + 1);
  if (How_ == Interpolation::Step) {
    for (size_t c = 0; c < Components_; c++) { out[c] = a[c]; }
    return;
  }
  if (How_ == Interpolation::Linear) {
    for (size_t c = 0; c < Components_; c++) { out[c] = a[c] + (b[c] - a[c]) * t; }
    return;
  }

  const double t2 = t * t;
  const double t3 = t2 * t;
  const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
  const double h10 = (t3 - 2.0 * t2 + t) * td;
  const double h01 = -2.0 * t3 + 3.0 * t2;
  const double h11 = (t3 - t2) * td;
  const double *m0 = OutTangent(k);
  const double *m1 = InTangent(k + 1);
  for (size_t c = 0; c < Components_; c++) {
    out[c] = h00 * a[c] + h10 * m0[c] + h01 * b[c] + h11 * m1[c];
  }
}

} // namespace outshine
