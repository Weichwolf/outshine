#ifndef OUTSHINE_BASE_SPATIAL_DRAPE_H
#define OUTSHINE_BASE_SPATIAL_DRAPE_H

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <unordered_map>

#include "Units.h"

namespace outshine {

inline constexpr size_t kDrapeRungs = 6;
inline constexpr double kFinestCellM = 32.0;
inline constexpr double kCellPerRung = 8.0;
inline constexpr double kDrapeGridM = 32.0;

[[nodiscard]] inline double DrapeCellM(size_t rung) {
  double cellM = kFinestCellM;
  for (size_t step = 0; step < rung; ++step) { cellM *= kCellPerRung; }
  return cellM;
}

using DrapeFaces = std::array<std::unordered_map<uint64_t, std::vector<uint32_t>>, kDrapeRungs>;

struct Drape {
  const std::unordered_map<uint64_t, float> &DrawnGround;
  const DrapeFaces &FacesAt;
  std::span<const float> InFrame;
  std::span<const uint32_t> Index;

  [[nodiscard]] double At(double eastM, double southM, double fallback) const;
};

inline double Drape::At(double eastM, double southM, double fallback) const {
  for (size_t rung = 0; rung < kDrapeRungs; ++rung) {
    const double cellM = DrapeCellM(rung);
    const auto cellE = static_cast<int64_t>(std::floor(eastM / cellM));
    const auto cellS = static_cast<int64_t>(std::floor(southM / cellM));
    const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
    const auto bucket = FacesAt[rung].find((atE << 32U) | atS);
    if (bucket != FacesAt[rung].end()) {
      for (const uint32_t at : bucket->second) {
        const size_t a = static_cast<size_t>(Index[at]) * 3u;
        const size_t b = static_cast<size_t>(Index[at + 1u]) * 3u;
        const size_t c = static_cast<size_t>(Index[at + 2u]) * 3u;
        if (c + 2 >= InFrame.size()) { continue; }
        const double aE = InFrame[a];
        const double aS = InFrame[a + 2];
        const double spanBE = static_cast<double>(InFrame[b]) - aE;
        const double spanBS = static_cast<double>(InFrame[b + 2]) - aS;
        const double spanCE = static_cast<double>(InFrame[c]) - aE;
        const double spanCS = static_cast<double>(InFrame[c + 2]) - aS;
        const double twice = spanBE * spanCS - spanCE * spanBS;
        if (std::fabs(twice) < kLeastTurnRad) { continue; }
        const double intoE = eastM - aE;
        const double intoS = southM - aS;
        const double towardB = (intoE * spanCS - spanCE * intoS) / twice;
        const double towardC = (spanBE * intoS - intoE * spanBS) / twice;
        if (towardB < -kLeastRunM || towardC < -kLeastRunM ||
            towardB + towardC > 1.0 + kLeastRunM) {
          continue;
        }
        return static_cast<double>(InFrame[a + 1]) * (1.0 - towardB - towardC) +
               static_cast<double>(InFrame[b + 1]) * towardB +
               static_cast<double>(InFrame[c + 1]) * towardC;
      }
    }
  }
  const double atE = eastM / kDrapeGridM;
  const double atS = southM / kDrapeGridM;
  const auto west = static_cast<int64_t>(std::floor(atE));
  const auto north = static_cast<int64_t>(std::floor(atS));
  const double alongE = atE - static_cast<double>(west);
  const double alongS = atS - static_cast<double>(north);
  const auto held = [this](int64_t east, int64_t south, double *out) {
    const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
    const uint64_t key = (atE << 32U) | atS;
    const auto stood = DrawnGround.find(key);
    if (stood == DrawnGround.end()) { return false; }
    *out = static_cast<double>(stood->second);
    return true;
  };
  std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
  if (held(west, north, corner.data()) && held(west + 1, north, &corner[1]) &&
      held(west, north + 1, &corner[2]) && held(west + 1, north + 1, &corner[3])) {
    const double above = corner[0] + (corner[1] - corner[0]) * alongE;
    const double below = corner[2] + (corner[3] - corner[2]) * alongE;
    return above + (below - above) * alongS;
  }
  const auto east = static_cast<int64_t>(std::llround(atE));
  const auto south = static_cast<int64_t>(std::llround(atS));
  double summed = 0.0;
  size_t took = 0;
  for (int64_t dy = -1; dy <= 1; ++dy) {
    for (int64_t dx = -1; dx <= 1; ++dx) {
      double stood = 0.0;
      if (!held(east + dx, south + dy, &stood)) { continue; }
      summed += stood;
      ++took;
    }
  }
  return took > 0 ? summed / static_cast<double>(took) : fallback;
}
} // namespace outshine
#endif
