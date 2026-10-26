#ifndef OUTSHINE_WORLD_GROUND_SHAPEDGROUND_H
#define OUTSHINE_WORLD_GROUND_SHAPEDGROUND_H
#include <cstdint>
#include <string>

namespace outshine::Ground {
struct ShapedGround {
  std::string Kind;
  double AmplitudeM = 0.0;
  double WavelengthM = 0.0;
  double Gradient = 0.0;
  double BearingDeg = 0.0;
  double FocusLatDeg = 0.0;
  double FocusLonDeg = 0.0;
  uint64_t Seed = 0;

  [[nodiscard]] bool operator==(const ShapedGround &) const noexcept = default;
};
}
#endif
