#ifndef OUTSHINE_IMPORT_TRACK_H
#define OUTSHINE_IMPORT_TRACK_H

#include <span>
#include <cstddef>
#include <vector>

#include "Types.h"

namespace outshine::Gltf {

class Track {
public:
  Track() = default;

  [[nodiscard]] static bool Build(AnimationPath path, Interpolation how,
                                  std::span<const double> times,
                                  std::span<const double> values, Track &out);

  [[nodiscard]] bool Valid() const { return Curve_.Valid(); }
  size_t Components() const { return Curve_.Components(); }
  size_t KeyframeCount() const { return Curve_.Count(); }

  void At(double seconds, double *out) const;

private:
  outshine::Keyframes Curve_;
  bool Spherical_ = false;
};

}
#endif
