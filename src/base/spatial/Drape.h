#ifndef OUTSHINE_BASE_SPATIAL_DRAPE_H
#define OUTSHINE_BASE_SPATIAL_DRAPE_H

#include <array>
#include <optional>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>
#include <limits>
#include <unordered_map>

#include "FlatMap.h"
#include "math/Units.h"

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

using DrapeFaces = std::array<FlatMap<std::vector<uint32_t>>, kDrapeRungs>;

struct Drape {
  const FlatMap<float> &DrawnGround;
  const DrapeFaces &FacesAt;
  std::span<const float> InFrame;
  std::span<const uint32_t> Index;

  struct EastSouth {
    double EastM = 0.0;
    double SouthM = 0.0;
  };

  struct Cell {
    int64_t East = 0;
    int64_t South = 0;
  };

  [[nodiscard]] std::optional<double> OnAFace(EastSouth at) const;
  [[nodiscard]] std::optional<double> BetweenFourCells(EastSouth at) const;
  [[nodiscard]] std::optional<double> AroundNineCells(EastSouth at) const;
  [[nodiscard]] bool CellAt(Cell at, double *out) const;
  [[nodiscard]] double At(EastSouth at, double fallback) const;
};

inline std::optional<double> Drape::OnAFace(EastSouth at) const {
  for (size_t rung = 0; rung < kDrapeRungs; ++rung) {
    const double cellM = DrapeCellM(rung);
    const auto cellE = static_cast<int64_t>(std::floor(at.EastM / cellM));
    const auto cellS = static_cast<int64_t>(std::floor(at.SouthM / cellM));
    const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
    const std::vector<uint32_t> *const bucket = FacesAt[rung].Find((atE << 32U) | atS);
    if (bucket == nullptr) { continue; }
    for (const uint32_t face : *bucket) {
      const size_t a = static_cast<size_t>(Index[face]) * 3u;
      const size_t b = static_cast<size_t>(Index[face + 1u]) * 3u;
      const size_t c = static_cast<size_t>(Index[face + 2u]) * 3u;
      if (c + 2 >= InFrame.size()) { continue; }
      const double aE = InFrame[a];
      const double aS = InFrame[a + 2];
      const double spanBE = static_cast<double>(InFrame[b]) - aE;
      const double spanBS = static_cast<double>(InFrame[b + 2]) - aS;
      const double spanCE = static_cast<double>(InFrame[c]) - aE;
      const double spanCS = static_cast<double>(InFrame[c + 2]) - aS;
      const double twice = spanBE * spanCS - spanCE * spanBS;
      if (std::fabs(twice) < kLeastTurnRad) { continue; }
      const double intoE = at.EastM - aE;
      const double intoS = at.SouthM - aS;
      const double towardB = (intoE * spanCS - spanCE * intoS) / twice;
      const double towardC = (spanBE * intoS - intoE * spanBS) / twice;
      if (towardB < -kLeastRunM || towardC < -kLeastRunM || towardB + towardC > 1.0 + kLeastRunM) {
        continue;
      }
      return static_cast<double>(InFrame[a + 1]) * (1.0 - towardB - towardC) +
             static_cast<double>(InFrame[b + 1]) * towardB +
             static_cast<double>(InFrame[c + 1]) * towardC;
    }
  }
  return std::nullopt;
}

inline bool Drape::CellAt(Cell at, double *out) const {
  const auto atE = static_cast<uint64_t>(at.East + 0x20000000LL);
  const auto atS = static_cast<uint64_t>(at.South + 0x20000000LL);
  const float *const stood = DrawnGround.Find((atE << 32U) | atS);
  if (stood == nullptr) { return false; }
  *out = static_cast<double>(*stood);
  return true;
}

inline std::optional<double> Drape::BetweenFourCells(EastSouth at) const {
  const double atE = at.EastM / kDrapeGridM;
  const double atS = at.SouthM / kDrapeGridM;
  const auto west = static_cast<int64_t>(std::floor(atE));
  const auto north = static_cast<int64_t>(std::floor(atS));
  std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
  if (!CellAt({.East = west, .South = north}, corner.data()) ||
      !CellAt({.East = west + 1, .South = north}, &corner[1]) ||
      !CellAt({.East = west, .South = north + 1}, &corner[2]) ||
      !CellAt({.East = west + 1, .South = north + 1}, &corner[3])) {
    return std::nullopt;
  }
  const double alongE = atE - static_cast<double>(west);
  const double alongS = atS - static_cast<double>(north);
  const double above = corner[0] + (corner[1] - corner[0]) * alongE;
  const double below = corner[2] + (corner[3] - corner[2]) * alongE;
  return above + (below - above) * alongS;
}

inline std::optional<double> Drape::AroundNineCells(EastSouth at) const {
  const auto east = static_cast<int64_t>(std::llround(at.EastM / kDrapeGridM));
  const auto south = static_cast<int64_t>(std::llround(at.SouthM / kDrapeGridM));
  double summed = 0.0;
  size_t took = 0;
  for (int64_t dy = -1; dy <= 1; ++dy) {
    for (int64_t dx = -1; dx <= 1; ++dx) {
      double stood = 0.0;
      if (!CellAt({.East = east + dx, .South = south + dy}, &stood)) { continue; }
      summed += stood;
      ++took;
    }
  }
  if (took == 0) { return std::nullopt; }
  return summed / static_cast<double>(took);
}

inline double Drape::At(EastSouth at, double fallback) const {
  if (const std::optional<double> onAFace = OnAFace(at)) { return *onAFace; }
  if (const std::optional<double> between = BetweenFourCells(at)) { return *between; }
  return AroundNineCells(at).value_or(fallback);
}

} // namespace outshine
#endif
