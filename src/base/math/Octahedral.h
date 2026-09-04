#ifndef OUTSHINE_BASE_MATH_OCTAHEDRAL_H
#define OUTSHINE_BASE_MATH_OCTAHEDRAL_H

#include <array>
#include <cmath>
#include <cstdint>

namespace outshine {

[[nodiscard]] inline std::array<float, 2> OctFolded(const std::array<float, 3> &unit) {
  const float sum = std::fabs(unit[0]) + std::fabs(unit[1]) + std::fabs(unit[2]);
  const float by = sum > 0.0f ? 1.0f / sum : 0.0f;
  float alongU = unit[0] * by;
  float alongV = unit[1] * by;
  if (unit[2] < 0.0f) {
    const float wasU = alongU;
    alongU = (1.0f - std::fabs(alongV)) * (wasU >= 0.0f ? 1.0f : -1.0f);
    alongV = (1.0f - std::fabs(wasU)) * (alongV >= 0.0f ? 1.0f : -1.0f);
  }
  return {{alongU, alongV}};
}

[[nodiscard]] inline std::array<float, 3> OctUnfolded(const std::array<float, 2> &folded) {
  std::array<float, 3> unit = {
      {folded[0], folded[1], 1.0f - std::fabs(folded[0]) - std::fabs(folded[1])}};
  if (unit[2] < 0.0f) {
    const float wasX = unit[0];
    unit[0] = (1.0f - std::fabs(unit[1])) * (wasX >= 0.0f ? 1.0f : -1.0f);
    unit[1] = (1.0f - std::fabs(wasX)) * (unit[1] >= 0.0f ? 1.0f : -1.0f);
  }
  const float len = std::sqrt(unit[0] * unit[0] + unit[1] * unit[1] + unit[2] * unit[2]);
  const float by = len > 0.0f ? 1.0f / len : 0.0f;
  return {{unit[0] * by, unit[1] * by, unit[2] * by}};
}

[[nodiscard]] inline uint32_t PackedPair(const std::array<float, 2> &pair) {
  const auto quantise = [](float held) {
    const float bounded = held < -1.0f ? -1.0f : (held > 1.0f ? 1.0f : held);
    return static_cast<uint32_t>(
        static_cast<uint16_t>(std::lround((bounded * 0.5f + 0.5f) * 65535.0f)));
  };
  return (quantise(pair[0]) << 16U) | quantise(pair[1]);
}

[[nodiscard]] inline std::array<float, 2> UnpackedPair(uint32_t word) {
  const auto held = [](uint32_t part) {
    return (static_cast<float>(part) / 65535.0f) * 2.0f - 1.0f;
  };
  return {{held(word >> 16U), held(word & 0xffffU)}};
}

} // namespace outshine
#endif
