#include "Keyframes.h"
#include <cstddef>
#include <cmath>
#include <expected>
#include <span>

namespace outshine {

std::expected<Keyframes, Keyframes::Error> Keyframes::Build(Interpolation how,
                                                            std::span<const double> frames,
                                                            std::span<const double> values,
                                                            size_t components) noexcept {
  switch (how) {
    case Interpolation::Step:
    case Interpolation::Linear:
    case Interpolation::CubicSpline: break;
    default: return std::unexpected(Error::InvalidInterpolation);
  }
  const size_t perKeyframe = how == Interpolation::CubicSpline ? 3u : 1u;
  if (frames.empty() || components == 0 || values.size() % components != 0 ||
      (values.size() / components) % perKeyframe != 0 ||
      values.size() / components / perKeyframe != frames.size() ||
      (how == Interpolation::CubicSpline && frames.size() < 2)) {
    return std::unexpected(Error::InvalidDimensions);
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    if (!std::isfinite(frames[i]) || (i > 0 && frames[i] <= frames[i - 1])) {
      return std::unexpected(Error::InvalidTimeGrid);
    }
  }
  if (!std::isfinite(frames.back() - frames.front())) {
    return std::unexpected(Error::InvalidTimeGrid);
  }
  for (const double value : values) {
    if (!std::isfinite(value)) { return std::unexpected(Error::NonFiniteValue); }
  }
  Keyframes result;
  result.Frames_ = frames;
  result.Values_ = values;
  result.Components_ = components;
  result.How_ = how;
  return result;
}

size_t Keyframes::Segment(double abscissa) const {
  size_t lo = 0;
  size_t hi = Frames_.size() - 1;
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
  if (!Valid() || !std::isfinite(abscissa) || Frames_.size() < 2) { return false; }
  if (abscissa <= Frames_[0] || abscissa >= Frames_[Frames_.size() - 1]) { return false; }
  keyframe = Segment(abscissa);
  const double td = Frames_[keyframe + 1] - Frames_[keyframe];

  weight = (td > 0.0) ? (abscissa - Frames_[keyframe]) / td : 0.0;
  return true;
}

void Keyframes::At(double abscissa, std::span<double> out) const {
  if (!Valid() || !std::isfinite(abscissa) || out.size() < Components_) { return; }
  if (Frames_.size() == 1 || abscissa <= Frames_[0]) {
    const double *v = Value(0);
    for (size_t c = 0; c < Components_; c++) { out[c] = v[c]; }
    return;
  }
  if (abscissa >= Frames_[Frames_.size() - 1]) {
    const double *v = Value(Frames_.size() - 1);
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

}
