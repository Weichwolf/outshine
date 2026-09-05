#include <chrono>
#include <cstdio>
#include "FlatMap.h"
#include "math/Units.h"
#include "GroundYield.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ratio>
#include <span>
#include <utility>
#include <vector>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace outshine {

namespace {

constexpr size_t kMostDenseCells = 1u << 22u;

constexpr double kBucketM = 32.0;

class CellGrid {
public:
  static constexpr int64_t kBias = 0x20000000LL;
  static constexpr int64_t kSpreadsAnywhere = std::numeric_limits<int64_t>::max();

  struct Sized {
    double CellM = 1.0;
    int64_t MostCells = kSpreadsAnywhere;
  };

  explicit CellGrid(Sized over) : CellM_(over.CellM), MostCells_(over.MostCells) {}

  [[nodiscard]] int64_t CellOf(double metres) const {
    return static_cast<int64_t>(std::floor(metres / CellM_));
  }

  static constexpr uint64_t KeyOf(int64_t cellE, int64_t cellS) {
    return (static_cast<uint64_t>(cellE + kBias) << 32U) | static_cast<uint64_t>(cellS + kBias);
  }

  [[nodiscard]] uint64_t KeyAt(EastSouth at) const {
    return KeyOf(CellOf(at.EastM), CellOf(at.SouthM));
  }

  void Expects(size_t entries) {
    Held_.reserve(entries);
    What_.reserve(entries);
  }

  void Spread(EastSouth low, EastSouth high, uint32_t what) {
    const int64_t fromE = CellOf(low.EastM);
    const int64_t toE = CellOf(high.EastM);
    const int64_t fromS = CellOf(low.SouthM);
    const int64_t toS = CellOf(high.SouthM);
    if ((toE - fromE + 1) * (toS - fromS + 1) > MostCells_) { return; }
    for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
      for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
        Held_.push_back(KeyOf(cellE, cellS));
        What_.push_back(what);
      }
    }
    Settled_ = false;
  }

  void Settles() {
    Settled_ = true;
    First_.clear();
    Seats_.clear();
    Wide_ = 0;
    if (Held_.empty()) { return; }
    LowE_ = std::numeric_limits<int64_t>::max();
    LowS_ = std::numeric_limits<int64_t>::max();
    int64_t highE = std::numeric_limits<int64_t>::min();
    int64_t highS = std::numeric_limits<int64_t>::min();
    for (const uint64_t key : Held_) {
      const auto cellE = static_cast<int64_t>(key >> 32U) - kBias;
      const auto cellS = static_cast<int64_t>(key & 0xffffffffU) - kBias;
      LowE_ = std::min(LowE_, cellE);
      LowS_ = std::min(LowS_, cellS);
      highE = std::max(highE, cellE);
      highS = std::max(highS, cellS);
    }
    const int64_t wide = highE - LowE_ + 1;
    const size_t cells = static_cast<size_t>(wide) * static_cast<size_t>(highS - LowS_ + 1);
    if (cells > kMostDenseCells) { return; }
    Wide_ = wide;
    First_.assign(cells + 1u, 0u);
    for (const uint64_t key : Held_) { ++First_[SeatOf(key) + 1u]; }
    for (size_t cell = 1; cell < First_.size(); ++cell) { First_[cell] += First_[cell - 1u]; }
    Seats_.resize(Held_.size());
    std::vector<uint32_t> at(First_.begin(), First_.end() - 1);
    for (size_t entry = 0; entry < Held_.size(); ++entry) {
      Seats_[at[SeatOf(Held_[entry])]++] = What_[entry];
    }
  }

  [[nodiscard]] std::span<const uint32_t> At(EastSouth where) const {
    if (Wide_ == 0) { return {}; }
    const int64_t cellE = CellOf(where.EastM) - LowE_;
    const int64_t cellS = CellOf(where.SouthM) - LowS_;
    if (cellE < 0 || cellS < 0 || cellE >= Wide_) { return {}; }
    const size_t cell =
        static_cast<size_t>(cellS) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
    if (cell + 1u >= First_.size()) { return {}; }
    return {Seats_.data() + First_[cell], First_[cell + 1u] - First_[cell]};
  }

  [[nodiscard]] bool Empty() const { return Held_.empty(); }

private:
  [[nodiscard]] size_t SeatOf(uint64_t key) const {
    const int64_t cellE = static_cast<int64_t>(key >> 32U) - kBias - LowE_;
    const int64_t cellS = static_cast<int64_t>(key & 0xffffffffU) - kBias - LowS_;
    return static_cast<size_t>(cellS) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
  }

  double CellM_;
  int64_t MostCells_;
  bool Settled_ = false;
  int64_t LowE_ = 0;
  int64_t LowS_ = 0;
  int64_t Wide_ = 0;
  std::vector<uint64_t> Held_;
  std::vector<uint32_t> What_;
  std::vector<uint32_t> First_;
  std::vector<uint32_t> Seats_;
};

CellGrid BucketOver(std::span<const Yields> these) {
  CellGrid out({.CellM = kBucketM});
  for (size_t at = 0; at < these.size(); ++at) {
    const Yields &one = these[at];
    out.Spread({.EastM = one.LowE - one.ApronM, .SouthM = one.LowS - one.ApronM},
               {.EastM = one.HighE + one.ApronM, .SouthM = one.HighS + one.ApronM},
               static_cast<uint32_t>(at));
  }
  out.Settles();
  return out;
}

double OutsideRingM(const Yields &held, EastSouth at) {
  const size_t corners = held.RingEastSouthM.size() / 2u;
  if (corners < 3) { return kBeyondAnyCoordinate; }
  bool inside = false;
  double nearest = kBeyondAnyCoordinate;
  for (size_t edge = 0, last = corners - 1u; edge < corners; last = edge++) {
    const double aE = held.RingEastSouthM[edge * 2u];
    const double aS = held.RingEastSouthM[edge * 2u + 1u];
    const double bE = held.RingEastSouthM[last * 2u];
    const double bS = held.RingEastSouthM[last * 2u + 1u];
    if ((aS > at.SouthM) != (bS > at.SouthM) &&
        at.EastM < (bE - aE) * (at.SouthM - aS) / (bS - aS) + aE) {
      inside = !inside;
    }
    const double runE = bE - aE;
    const double runS = bS - aS;
    const double runM = runE * runE + runS * runS;
    const double part =
        runM > kLeastTurnRad
            ? std::clamp(((at.EastM - aE) * runE + (at.SouthM - aS) * runS) / runM, 0.0, 1.0)
            : 0.0;
    const double offE = at.EastM - (aE + runE * part);
    const double offS = at.SouthM - (aS + runS * part);
    nearest = std::min(nearest, std::sqrt(offE * offE + offS * offS));
  }
  return inside ? -nearest : nearest;
}

struct Pressing {
  double WantedM = 0.0;
  bool Moves = false;
  uint32_t Which = 0;
};

Pressing PressesAt(std::span<const Yields> these,
                   std::span<const uint32_t> over,
                   std::span<const uint8_t> structures,
                   EastSouth at,
                   double wasM) {
  double lowest = wasM;
  double highest = wasM;
  double roofM = kBeyondAnyCoordinate;
  double basinM = wasM;
  bool landHeld = false;
  uint32_t lowestBy = 0;
  uint32_t highestBy = 0;
  uint32_t basinBy = 0;
  for (const uint32_t which : over) {
    if (!structures.empty() && structures[which] != 0u) { continue; }
    const Yields &held = these[which];
    if (at.EastM < held.LowE - held.ApronM || at.EastM > held.HighE + held.ApronM ||
        at.SouthM < held.LowS - held.ApronM || at.SouthM > held.HighS + held.ApronM) {
      continue;
    }
    const double signedM = OutsideRingM(held, at);
    const double onRoad = held.WantsAt(at);
    if (held.Basin) {
      if (signedM > 0.0) { continue; }
      const double bankAt = onRoad + std::max(0.0, held.ApronM + signedM) * kBatterRise;
      if (bankAt < basinM) {
        basinM = bankAt;
        basinBy = which;
      }
      continue;
    }
    const double out = std::max(signedM, 0.0);
    if (out > held.ApronM) { continue; }
    landHeld = true;
    const double cutAt = onRoad + out * kBatterRise;
    if (cutAt < lowest) {
      lowest = cutAt;
      lowestBy = which;
    }
    roofM = std::min(roofM, cutAt);
    if (held.Fills) {
      const double fillAt = onRoad - out * kBatterRise;
      if (fillAt > highest) {
        highest = fillAt;
        highestBy = which;
      }
    }
  }
  if (!landHeld && basinM < lowest) {
    lowest = basinM;
    lowestBy = basinBy;
  }
  if (lowest < wasM) { return {.WantedM = lowest, .Moves = true, .Which = lowestBy}; }
  if (highest > wasM) {
    const double wanted = std::min(highest, roofM);
    return {.WantedM = wanted, .Moves = wanted > wasM, .Which = highestBy};
  }
  return {};
}

} // namespace

Pressed PressPoints(std::span<const Yields> these,
                    std::span<const EastSouth> at,
                    std::span<double> upM,
                    double mostEarthworkM) {
  Pressed told;
  if (these.empty() || at.size() != upM.size()) { return told; }
  const CellGrid buckets = BucketOver(these);
  std::vector<uint8_t> structures(these.size(), 0u);
  for (size_t one = 0; one < at.size(); ++one) {
    const Pressing under = PressesAt(these, buckets.At(at[one]), {}, at[one], upM[one]);
    if (under.Moves && std::fabs(under.WantedM - upM[one]) > mostEarthworkM) {
      structures[under.Which] = 1u;
    }
  }
  for (const uint8_t one : structures) { told.Structures += one; }
  for (size_t one = 0; one < at.size(); ++one) {
    const Pressing under = PressesAt(these, buckets.At(at[one]), structures, at[one], upM[one]);
    if (!under.Moves) { continue; }
    if (std::fabs(under.WantedM - upM[one]) > mostEarthworkM) {
      ++told.Held;
      continue;
    }
    upM[one] = under.WantedM;
    ++told.Moved;
  }
  return told;
}

} // namespace outshine
