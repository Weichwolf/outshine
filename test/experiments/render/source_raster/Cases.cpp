#include "Input.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace SourceRaster {
std::vector<Quad> Generate(uint32_t seed, bool normalize) {
  constexpr uint32_t FractionMask = 0x007fffff;
  constexpr uint32_t Half = 0x3f000000;
  constexpr uint32_t Two = 0x40000000;
  constexpr uint32_t CalibrationCases = 16;
  constexpr std::array Scales{-126, -125, -120, -80, -40, 0, 40, 80, 120, 125, 126};
  std::minstd_rand random(seed);
  std::vector<Quad> result;
  result.reserve(CaseCount);
  for (uint32_t i = 0; i < CaseCount; ++i) {
    const auto numerator =
        std::bit_cast<float>(Half | (static_cast<uint32_t>(random()) & FractionMask));
    const auto denominator =
        std::bit_cast<float>(Two | (static_cast<uint32_t>(random()) & FractionMask));
    const int scale = Scales[i % Scales.size()];
    const float z = i < CalibrationCases ? .25f : std::ldexp(numerator, scale);
    const float w = i < CalibrationCases ? 1.f : std::ldexp(denominator, scale);
    const uint32_t row = i / Tiles;
    const auto left = static_cast<float>(i % Tiles * TileSide + 1);
    const auto top = static_cast<float>(row * TileSide + 1);
    const float right = left + static_cast<float>(TileSide - 2);
    const float bottom = top + static_cast<float>(TileSide - 2);
    constexpr float halfSide = static_cast<float>(Side) / 2;
    result.push_back(
        {.Points = {{{(left / halfSide - 1) * w, (1 - top / halfSide) * w, z, w},
                     {(right / halfSide - 1) * w, (1 - top / halfSide) * w, z, w},
                     {(right / halfSide - 1) * w, (1 - bottom / halfSide) * w, z, w},
                     {(left / halfSide - 1) * w, (1 - bottom / halfSide) * w, z, w}}}});
  }
  if (normalize) {
    for (auto &quad : result) {
      const int shift = -std::ilogb(quad.Points[0][3]);
      for (auto &point : quad.Points) {
        for (auto &value : point) { value = std::scalbn(value, shift); }
      }
    }
  }
  return result;
}
}
