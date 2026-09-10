#include "math/Vec4.h"
#include "Track.h"
#include "Keyframes.h"

#include <cmath>
#include <cstddef>
#include <span>

namespace outshine::Gltf {

namespace {

constexpr double kSameRotationCosine = 1.0 - 1e-12;

void Normalise(std::span<double> quaternion) {
  const double square = quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                        quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3];
  if (!(square > 0)) { return; }
  const double magnitude = std::sqrt(square);
  for (size_t at = 0; at < 4; ++at) { quaternion[at] /= magnitude; }
}

void Slerp(const double *from, const double *to, double weight, std::span<double> out) {
  double cosine = from[0] * to[0] + from[1] * to[1] + from[2] * to[2] + from[3] * to[3];
  Vec4 target = {{to[0], to[1], to[2], to[3]}};
  if (cosine < 0) {
    cosine = -cosine;
    for (double &at : target) { at = -at; }
  }
  if (cosine > kSameRotationCosine) {
    for (size_t at = 0; at < 4; ++at) { out[at] = from[at] + (target[at] - from[at]) * weight; }
    Normalise(out);
    return;
  }
  const double angle = std::acos(cosine);
  const double sine = std::sin(angle);
  const double before = std::sin((1.0 - weight) * angle) / sine;
  const double after = std::sin(weight * angle) / sine;
  for (size_t at = 0; at < 4; ++at) { out[at] = from[at] * before + target[at] * after; }
  Normalise(out);
}

}

bool Track::Build(AnimationPath path,
                  Interpolation how,
                  std::span<const double> times,
                  std::span<const double> values,
                  Track &out) {
  if (times.empty() || times.front() < 0.0) { return false; }
  switch (path) {
    case AnimationPath::Translation:
    case AnimationPath::Rotation:
    case AnimationPath::Scale:
    case AnimationPath::Weights:
    case AnimationPath::MaterialFactor: break;
    default: return false;
  }
  const size_t perKeyframe = how == Interpolation::CubicSpline ? 3u : 1u;
  size_t components = PathComponents(path);
  if (components == 0) {
    if (values.size() % times.size() != 0 || (values.size() / times.size()) % perKeyframe != 0) {
      return false;
    }
    components = values.size() / times.size() / perKeyframe;
  }
  const auto curve = Keyframes::Build(how, times, values, components);
  if (!curve) { return false; }
  Track candidate;
  candidate.Curve_ = *curve;
  candidate.Spherical_ = path == AnimationPath::Rotation;
  out = candidate;
  return true;
}

void Track::At(double seconds, std::span<double> out) const {
  if (!Valid() || !std::isfinite(seconds) || out.size() < Components()) { return; }
  size_t span = 0;
  double weight = 0.0;

  const bool spherical =
      Spherical_ && Curve_.How() == Interpolation::Linear && Curve_.Span(seconds, span, weight);
  if (spherical) {
    Slerp(Curve_.ValueAt(span), Curve_.ValueAt(span + 1), weight, out);
    return;
  }
  Curve_.At(seconds, out);
  if (Spherical_) { Normalise(out); }
}

}
