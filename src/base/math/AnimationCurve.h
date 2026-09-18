#ifndef OUTSHINE_BASE_MATH_ANIMATIONCURVE_H
#define OUTSHINE_BASE_MATH_ANIMATIONCURVE_H

#include <span>
#include <cstddef>
#include <vector>

#include "Keyframes.h"

namespace outshine {

class AnimationCurve {
public:
  enum class Values { Linear, Rotation };

  [[nodiscard]] static bool Build(Keyframes::Interpolation how,
                                  std::span<const double> times,
                                  std::span<const double> values,
                                  size_t components,
                                  Values kind,
                                  AnimationCurve &out);

  [[nodiscard]] bool Valid() const { return Curve_.Valid(); }

  [[nodiscard]] size_t Components() const { return Curve_.Components(); }

  [[nodiscard]] size_t KeyframeCount() const { return Curve_.Count(); }

  void At(double seconds, std::span<double> out) const;

private:
  Keyframes Curve_;
  bool Spherical_ = false;
};

}
#endif
