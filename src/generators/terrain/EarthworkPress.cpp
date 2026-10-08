#include <chrono>
#include <cstdio>
#include "FlatMap.h"
#include "math/Units.h"
#include "EarthworkPress.h"
#include "ProfiledCorridorPress.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
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

  static constexpr uint64_t KeyOf(int64_t cellE, int64_t cellN) {
    return (static_cast<uint64_t>(cellE + kBias) << 32U) | static_cast<uint64_t>(cellN + kBias);
  }

  [[nodiscard]] uint64_t KeyAt(EastNorth at) const {
    return KeyOf(CellOf(at.EastM), CellOf(at.NorthM));
  }

  void Expects(size_t entries) {
    Held_.reserve(entries);
    What_.reserve(entries);
  }

  void Spread(EastNorth low, EastNorth high, uint32_t what) {
    const int64_t fromE = CellOf(low.EastM);
    const int64_t toE = CellOf(high.EastM);
    const int64_t fromN = CellOf(low.NorthM);
    const int64_t toN = CellOf(high.NorthM);
    if ((toE - fromE + 1) * (toN - fromN + 1) > MostCells_) { return; }
    for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
      for (int64_t cellN = fromN; cellN <= toN; ++cellN) {
        Held_.push_back(KeyOf(cellE, cellN));
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
    LowN_ = std::numeric_limits<int64_t>::max();
    int64_t highE = std::numeric_limits<int64_t>::min();
    int64_t highN = std::numeric_limits<int64_t>::min();
    for (const uint64_t key : Held_) {
      const auto cellE = static_cast<int64_t>(key >> 32U) - kBias;
      const auto cellN = static_cast<int64_t>(key & 0xffffffffU) - kBias;
      LowE_ = std::min(LowE_, cellE);
      LowN_ = std::min(LowN_, cellN);
      highE = std::max(highE, cellE);
      highN = std::max(highN, cellN);
    }
    const int64_t wide = highE - LowE_ + 1;
    const size_t cells = static_cast<size_t>(wide) * static_cast<size_t>(highN - LowN_ + 1);
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

  [[nodiscard]] std::span<const uint32_t> At(EastNorth where) const {
    if (Wide_ == 0) { return {}; }
    const int64_t cellE = CellOf(where.EastM) - LowE_;
    const int64_t cellN = CellOf(where.NorthM) - LowN_;
    if (cellE < 0 || cellN < 0 || cellE >= Wide_) { return {}; }
    const size_t cell =
        static_cast<size_t>(cellN) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
    if (cell + 1u >= First_.size()) { return {}; }
    return {Seats_.data() + First_[cell], First_[cell + 1u] - First_[cell]};
  }

  [[nodiscard]] bool Empty() const { return Held_.empty(); }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return (Held_.capacity() * sizeof(uint64_t)) + (What_.capacity() * sizeof(uint32_t)) +
           (First_.capacity() * sizeof(uint32_t)) + (Seats_.capacity() * sizeof(uint32_t));
  }

private:
  [[nodiscard]] size_t SeatOf(uint64_t key) const {
    const int64_t cellE = static_cast<int64_t>(key >> 32U) - kBias - LowE_;
    const int64_t cellN = static_cast<int64_t>(key & 0xffffffffU) - kBias - LowN_;
    return static_cast<size_t>(cellN) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
  }

  double CellM_;
  int64_t MostCells_;
  bool Settled_ = false;
  int64_t LowE_ = 0;
  int64_t LowN_ = 0;
  int64_t Wide_ = 0;
  std::vector<uint64_t> Held_;
  std::vector<uint32_t> What_;
  std::vector<uint32_t> First_;
  std::vector<uint32_t> Seats_;
};

constexpr double kSmoothstepPeakDerivative = 1.875;
constexpr double kContactToleranceM = 1.e-6;

bool HasSoftApron(const EarthworkStamp &stamp) {
  return stamp.Kind == EarthworkKind::Pad || stamp.Kind == EarthworkKind::Corridor;
}

double ApronWidthM(const EarthworkStamp &stamp, double mostEarthworkM) {
  return stamp.Kind == EarthworkKind::Corridor
             ? std::hypot(stamp.ApronM,
                          kSmoothstepPeakDerivative *
                              std::min(std::abs(stamp.YieldM), mostEarthworkM) * kBatterRun)
             : stamp.ApronM;
}

CellGrid BucketOver(std::span<const EarthworkStamp> these, double mostEarthworkM) {
  CellGrid out({.CellM = kBucketM});
  for (size_t at = 0; at < these.size(); ++at) {
    const EarthworkStamp &one = these[at];
    const double apronM = ApronWidthM(one, mostEarthworkM);
    out.Spread({.EastM = one.LowE - apronM, .NorthM = one.LowN - apronM},
               {.EastM = one.HighE + apronM, .NorthM = one.HighN + apronM},
               static_cast<uint32_t>(at));
  }
  out.Settles();
  return out;
}

double SignedDistanceToRingM(std::span<const double> ring, EastNorth at) {
  const size_t corners = ring.size() / 2u;
  if (corners < 3) { return kBeyondAnyCoordinate; }
  bool inside = false;
  double nearest = kBeyondAnyCoordinate;
  for (size_t edge = 0, last = corners - 1u; edge < corners; last = edge++) {
    const double aE = ring[edge * 2u];
    const double aN = ring[edge * 2u + 1u];
    const double bE = ring[last * 2u];
    const double bN = ring[last * 2u + 1u];
    if ((aN > at.NorthM) != (bN > at.NorthM) &&
        at.EastM < (bE - aE) * (at.NorthM - aN) / (bN - aN) + aE) {
      inside = !inside;
    }
    const double runE = bE - aE;
    const double runN = bN - aN;
    const double runM = runE * runE + runN * runN;
    const double part =
        runM > kLeastTurnRad
            ? std::clamp(((at.EastM - aE) * runE + (at.NorthM - aN) * runN) / runM, 0.0, 1.0)
            : 0.0;
    const double offE = at.EastM - (aE + runE * part);
    const double offN = at.NorthM - (aN + runN * part);
    nearest = std::min(nearest, std::sqrt(offE * offE + offN * offN));
  }
  return inside ? -nearest : nearest;
}

double OutsideRingM(const EarthworkStamp &held, EastNorth at) {
  const double outer = SignedDistanceToRingM(held.RingEastNorthM, at);
  if (std::abs(outer) <= kContactToleranceM) { return 0; }
  if (outer >= 0.0) { return outer; }
  double insideM = -outer;
  for (const auto &hole : held.HoleRingsEastNorthM) {
    const double distanceM = SignedDistanceToRingM(hole, at);
    if (distanceM <= 0.0) { return -distanceM; }
    insideM = std::min(insideM, distanceM);
  }
  return -insideM;
}

struct Pressing {
  double WantedM = 0.0;
  bool Moves = false;
  uint32_t Which = 0;
  uint32_t CappedBy = kNoStamp;
};

struct Bids {
  double LowestM = 0.0;
  double HighestM = 0.0;
  double RoofM = kBeyondAnyCoordinate;
  double BasinM = 0.0;
  bool LandHeld = false;
  uint32_t LowestBy = 0;
  uint32_t HighestBy = 0;
  uint32_t BasinBy = 0;
  uint32_t RoofBy = kNoStamp;
};

struct CoveredNodes {
  uint32_t Point = 0;
  std::vector<EarthworkPointClaim> *Into = nullptr;
};

struct Bid {
  uint32_t Which = 0;
  double OutsideM = 0.0;
  double WantsM = 0.0;
};

uint64_t CorridorOf(const EarthworkStamp &stamp) {
  return stamp.Profile ? stamp.Profile->CorridorKey : stamp.CorridorKey;
}

double RoadBedAt(const EarthworkStamp &stamp, EastNorth at) {
  const double bedM = stamp.WantsAt(at);
  if (stamp.Kind != EarthworkKind::Corridor || stamp.CorridorKey == 0 ||
      stamp.RingEastNorthM.size() < 6) {
    return bedM;
  }
  double lowM = kBeyondAnyCoordinate;
  double highM = -kBeyondAnyCoordinate;
  for (size_t corner = 0; corner + 1 < stamp.RingEastNorthM.size(); corner += 2) {
    const double contactM = stamp.WantsAt(
        {.EastM = stamp.RingEastNorthM[corner], .NorthM = stamp.RingEastNorthM[corner + 1]});
    lowM = std::min(lowM, contactM);
    highM = std::max(highM, contactM);
  }
  return std::clamp(bedM, lowM, highM);
}

void BidsBasin(const EarthworkStamp &held, Bid bid, Bids *bids) {
  if (bid.OutsideM > 0.0) { return; }
  const double bankAt = bid.WantsM + std::max(0.0, held.ApronM + bid.OutsideM) * kBatterRise;
  if (bankAt < bids->BasinM) {
    bids->BasinM = bankAt;
    bids->BasinBy = bid.Which;
  }
}

void BidsTerrain(
    const EarthworkStamp &held, Bid bid, double wasM, double mostEarthworkM, Bids *bids) {
  double out = held.Profile ? 0.0 : std::max(bid.OutsideM, 0.0);
  if (!held.Profile && HasSoftApron(held)) {
    if (out > 0.0) {
      bid.WantsM = wasM + std::clamp(bid.WantsM - wasM, -mostEarthworkM, mostEarthworkM);
      const double apronM = ApronWidthM(held, mostEarthworkM);
      if (out >= apronM) { return; }
      bid.WantsM = std::lerp(bid.WantsM, wasM, ProfiledCorridor::Smoothstep(out / apronM));
      out = 0.0;
    }
  } else if (out > held.ApronM) {
    return;
  }
  bids->LandHeld = bids->LandHeld || (held.Kind != EarthworkKind::Clearance && bid.OutsideM <= 0.0);
  const double cutAt = bid.WantsM + out * kBatterRise;
  if (cutAt < bids->LowestM) {
    bids->LowestM = cutAt;
    bids->LowestBy = bid.Which;
  }
  if (cutAt < bids->RoofM) {
    bids->RoofM = cutAt;
    bids->RoofBy = bid.Which;
  }
  if (!held.Fills || held.Kind == EarthworkKind::Clearance) { return; }
  const double fillAt = bid.WantsM - out * kBatterRise;
  if (fillAt > bids->HighestM) {
    bids->HighestM = fillAt;
    bids->HighestBy = bid.Which;
  }
}

struct NearestProfile {
  ProfiledCorridor::Offer Offer;
  uint32_t Which;
  const ProfiledCorridorSpan *Profile;
};

const ProfiledCorridorSpan *ProfileOf(const EarthworkStamp &stamp) {
  return stamp.Profile.transform([](const ProfiledCorridorSpan &profile) { return &profile; })
      .value_or(nullptr);
}

void ProfilesAt(std::span<const EarthworkStamp> these,
                std::span<const uint32_t> over,
                std::span<const uint8_t> structures,
                EastNorth at,
                double mostEarthworkM,
                std::vector<NearestProfile> &profiles) {
  profiles.clear();
  for (const uint32_t which : over) {
    if (!structures.empty() && structures[which] != 0u) { continue; }
    const EarthworkStamp &held = these[which];
    const ProfiledCorridorSpan *profile = ProfileOf(held);
    if (profile == nullptr) { continue; }
    const double apronM = ApronWidthM(held, mostEarthworkM);
    if (at.EastM < held.LowE - apronM || at.EastM > held.HighE + apronM ||
        at.NorthM < held.LowN - apronM || at.NorthM > held.HighN + apronM) {
      continue;
    }
    const auto offered = ProfiledCorridor::OfferAt(*profile, apronM, at);
    if (!offered) { continue; }
    const auto same = std::ranges::find_if(profiles, [profile](const NearestProfile &candidate) {
      return candidate.Profile->CorridorKey == profile->CorridorKey;
    });
    const NearestProfile candidate{.Offer = *offered, .Which = which, .Profile = profile};
    if (same == profiles.end()) {
      profiles.push_back(candidate);
    } else if (offered->DistanceSquared < same->Offer.DistanceSquared ||
               (offered->DistanceSquared == same->Offer.DistanceSquared &&
                ProfiledCorridor::Earlier(*profile, *same->Profile))) {
      *same = candidate;
    }
  }
}

double ProfileBedAt(std::span<const EarthworkStamp> these,
                    std::span<const uint32_t> over,
                    std::span<const uint8_t> structures,
                    EastNorth at,
                    const NearestProfile &selected) {
  ProfiledCorridor::Average average;
  for (const uint32_t which : over) {
    if (!structures.empty() && structures[which] != 0u) { continue; }
    const EarthworkStamp &held = these[which];
    const ProfiledCorridorSpan *profile = ProfileOf(held);
    if (profile != nullptr && profile->CorridorKey == selected.Profile->CorridorKey) {
      ProfiledCorridor::Accumulate(
          *profile, held.ApronM, at, selected.Offer.DistanceSquared, average);
    }
  }
  if (average.Weight <= 0.0) { return selected.Offer.BedM; }
  const double distanceM = std::sqrt(selected.Offer.DistanceSquared);
  const double vergeM = selected.Offer.FullHalfWidthM - selected.Offer.PavementHalfWidthM;
  const double fraction =
      vergeM > 0.0 ? std::clamp((distanceM - selected.Offer.PavementHalfWidthM) / vergeM, 0.0, 1.0)
                   : 0.0;
  return std::lerp(selected.Offer.BedM,
                   average.HeightM / average.Weight,
                   ProfiledCorridor::Smoothstep(fraction));
}

std::optional<Bid> ProfileBidAt(std::span<const EarthworkStamp> these,
                                std::span<const uint32_t> over,
                                std::span<const uint8_t> structures,
                                EastNorth at,
                                double wasM,
                                double mostEarthworkM,
                                std::vector<NearestProfile> &profiles) {
  ProfilesAt(these, over, structures, at, mostEarthworkM, profiles);
  if (profiles.empty()) { return std::nullopt; }
  const auto selected = std::ranges::min_element(profiles, [](const auto &a, const auto &b) {
    return a.Offer.OutsideM < b.Offer.OutsideM ||
           (a.Offer.OutsideM == b.Offer.OutsideM &&
            ProfiledCorridor::Earlier(*a.Profile, *b.Profile));
  });
  if (selected->Offer.OutsideM <= 0.0) {
    return Bid{.Which = selected->Which,
               .OutsideM = selected->Offer.OutsideM,
               .WantsM = ProfileBedAt(these, over, structures, at, *selected)};
  }
  double weights = 1.0;
  double correctionM = 0.0;
  double greatestWeight = 0.0;
  uint32_t strongest = selected->Which;
  for (const auto &profile : profiles) {
    const double bedM = wasM + std::clamp(ProfileBedAt(these, over, structures, at, profile) - wasM,
                                          -mostEarthworkM,
                                          mostEarthworkM);
    const double apronM = ApronWidthM(these[profile.Which], mostEarthworkM);
    if (profile.Offer.OutsideM >= apronM) { continue; }
    const double fade = ProfiledCorridor::Smoothstep(profile.Offer.OutsideM / apronM);
    const double weight = (1.0 - fade) / fade;
    weights += weight;
    correctionM += weight * (bedM - wasM);
    if (weight > greatestWeight) {
      greatestWeight = weight;
      strongest = profile.Which;
    }
  }
  if (greatestWeight == 0.0) { return std::nullopt; }
  return Bid{.Which = strongest,
             .OutsideM = selected->Offer.OutsideM,
             .WantsM = wasM + correctionM / weights};
}

Bids WithoutOwnAprons(std::span<const EarthworkStamp> these,
                      std::span<const uint32_t> over,
                      std::span<const uint8_t> structures,
                      uint64_t corridor,
                      EastNorth at,
                      double wasM,
                      double mostEarthworkM,
                      const std::optional<Bid> &profiled) {
  Bids bounds{.LowestM = wasM, .HighestM = wasM, .BasinM = wasM};
  for (const uint32_t which : over) {
    if (!structures.empty() && structures[which] != 0u) { continue; }
    const EarthworkStamp &stamp = these[which];
    if (stamp.Profile || stamp.Kind == EarthworkKind::Basin) { continue; }
    const double outsideM = OutsideRingM(stamp, at);
    if (stamp.Kind == EarthworkKind::Corridor && stamp.CorridorKey == corridor && outsideM > 0) {
      continue;
    }
    BidsTerrain(stamp,
                {.Which = which, .OutsideM = outsideM, .WantsM = RoadBedAt(stamp, at)},
                wasM,
                mostEarthworkM,
                &bounds);
  }
  if (profiled) { BidsTerrain(these[profiled->Which], *profiled, wasM, mostEarthworkM, &bounds); }
  return bounds;
}

Pressing ChooseHeight(const Bids &bids, double wasM) {
  if (bids.LowestM < wasM) {
    return {.WantedM = bids.LowestM, .Moves = true, .Which = bids.LowestBy};
  }
  if (bids.HighestM > wasM) {
    const double wanted = std::min(bids.HighestM, bids.RoofM);
    return {.WantedM = wanted,
            .Moves = wanted > wasM,
            .Which = bids.HighestBy,
            .CappedBy = bids.RoofM < bids.HighestM ? bids.RoofBy : kNoStamp};
  }
  return {};
}

Pressing PressesAt(std::span<const EarthworkStamp> these,
                   std::span<const uint32_t> over,
                   std::span<const uint8_t> structures,
                   EastNorth at,
                   double wasM,
                   double mostEarthworkM,
                   CoveredNodes covered,
                   std::vector<NearestProfile> &profiles) {
  Bids bids{.LowestM = wasM, .HighestM = wasM, .BasinM = wasM};
  for (const uint32_t which : over) {
    if (!structures.empty() && structures[which] != 0u) { continue; }
    const EarthworkStamp &held = these[which];
    const double apronM = ApronWidthM(held, mostEarthworkM);
    if (at.EastM < held.LowE - apronM || at.EastM > held.HighE + apronM ||
        at.NorthM < held.LowN - apronM || at.NorthM > held.HighN + apronM) {
      continue;
    }
    if (held.Profile) { continue; }
    const Bid bid{
        .Which = which, .OutsideM = OutsideRingM(held, at), .WantsM = RoadBedAt(held, at)};
    if (held.Kind == EarthworkKind::Basin) {
      BidsBasin(held, bid, &bids);
      continue;
    }
    if (covered.Into != nullptr && bid.OutsideM < 0.0) {
      covered.Into->push_back({.Point = covered.Point, .Stamp = which});
    }
    BidsTerrain(held, bid, wasM, mostEarthworkM, &bids);
  }
  const auto profiled = ProfileBidAt(these, over, structures, at, wasM, mostEarthworkM, profiles);
  if (profiled) {
    const Bid bid = profiled.value();
    const EarthworkStamp &road = these[bid.Which];
    if (covered.Into != nullptr && bid.OutsideM < 0.0) {
      covered.Into->push_back({.Point = covered.Point, .Stamp = bid.Which});
    }
    BidsTerrain(road, bid, wasM, mostEarthworkM, &bids);
  }
  if (!bids.LandHeld && bids.BasinM < bids.LowestM) {
    bids.LowestM = bids.BasinM;
    bids.LowestBy = bids.BasinBy;
  }
  const uint64_t corridor = bids.HighestM > wasM ? CorridorOf(these[bids.HighestBy]) : 0;
  if (corridor != 0 && bids.RoofM < bids.HighestM && bids.HighestM > wasM &&
      CorridorOf(these[bids.RoofBy]) == corridor) {
    const auto bounds =
        WithoutOwnAprons(these, over, structures, corridor, at, wasM, mostEarthworkM, profiled);
    bids.RoofM = bounds.RoofM;
    bids.RoofBy = bounds.RoofBy;
  }
  return ChooseHeight(bids, wasM);
}

void RejectAt(std::span<const EarthworkStamp> these,
              const CellGrid &buckets,
              std::span<const EastNorth> at,
              std::span<const double> upM,
              std::span<uint8_t> structures,
              double mostEarthworkM,
              size_t one) {
  for (const uint32_t which : buckets.At(at[one])) {
    const EarthworkStamp &stamp = these[which];
    double outsideM = OutsideRingM(stamp, at[one]);
    double bedM = RoadBedAt(stamp, at[one]);
    if (stamp.Profile) {
      const auto offered = ProfiledCorridor::OfferAt(*stamp.Profile, stamp.ApronM, at[one]);
      if (!offered) { continue; }
      outsideM = offered->OutsideM;
      bedM = offered->BedM;
    }
    if (outsideM > stamp.ApronM) { continue; }
    const double batterM = std::max(0.0, outsideM) * kBatterRise;
    if (bedM + batterM - upM[one] < -mostEarthworkM ||
        (stamp.Fills && stamp.Kind != EarthworkKind::Clearance &&
         bedM - batterM - upM[one] > mostEarthworkM)) {
      structures[which] = 1u;
    }
  }
}

void ApplyAt(std::span<const EarthworkStamp> these,
             const CellGrid &buckets,
             std::span<const EastNorth> at,
             std::span<double> upM,
             std::span<const uint8_t> structures,
             double mostEarthworkM,
             EarthworkPressResult &told,
             size_t one,
             std::vector<NearestProfile> &profiles) {
  const Pressing under = PressesAt(these,
                                   buckets.At(at[one]),
                                   structures,
                                   at[one],
                                   upM[one],
                                   mostEarthworkM,
                                   {.Point = static_cast<uint32_t>(one), .Into = &told.Inside},
                                   profiles);
  if (!under.Moves) {
    told.DecidedBy[one] = under.CappedBy;
    return;
  }
  if (std::fabs(under.WantedM - upM[one]) > mostEarthworkM) {
    told.DecidedBy[one] = kHeldStamp;
    ++told.Held;
    return;
  }
  told.DecidedBy[one] = under.CappedBy != kNoStamp ? under.CappedBy : under.Which;
  upM[one] = under.WantedM;
  ++told.Moved;
}

}

EarthworkPressResult ApplyEarthworkStamps(std::span<const EarthworkStamp> these,
                                          std::span<const EastNorth> at,
                                          std::span<double> upM,
                                          double mostEarthworkM) {
  EarthworkPressResult told;
  if (these.empty() || at.size() != upM.size()) { return told; }
  const auto began = std::chrono::steady_clock::now();
  const CellGrid buckets = BucketOver(these, mostEarthworkM);
  const auto bucketed = std::chrono::steady_clock::now();
  std::vector<uint8_t> structures(these.size(), 0u);
  std::vector<NearestProfile> profiles;
  for (size_t one = 0; one < at.size(); ++one) {
    RejectAt(these, buckets, at, upM, structures, mostEarthworkM, one);
  }
  const auto rejected = std::chrono::steady_clock::now();
  for (const uint8_t one : structures) { told.Structures += one; }
  told.DecidedBy.assign(at.size(), kNoStamp);
  for (size_t one = 0; one < at.size(); ++one) {
    ApplyAt(these, buckets, at, upM, structures, mostEarthworkM, told, one, profiles);
  }
  told.Refused = std::move(structures);
  const auto applied = std::chrono::steady_clock::now();
  told.BucketMs = std::chrono::duration<double, std::milli>(bucketed - began).count();
  told.RejectMs = std::chrono::duration<double, std::milli>(rejected - bucketed).count();
  told.ApplyMs = std::chrono::duration<double, std::milli>(applied - rejected).count();
  return told;
}

struct EarthworkPressJob::State {
  enum class Phase : uint8_t { Reject, InitializeDecisions, Apply, Done };

  std::span<const EarthworkStamp> These;
  std::span<const EastNorth> At;
  std::span<double> UpM;
  double MostEarthworkM;
  std::chrono::steady_clock::time_point Began = std::chrono::steady_clock::now();
  CellGrid Buckets;
  std::vector<uint8_t> Structures;
  std::vector<NearestProfile> Profiles;
  EarthworkPressResult Result;
  size_t Next = 0;
  Phase Current = Phase::Reject;

  State(std::span<const EarthworkStamp> these,
        std::span<const EastNorth> at,
        std::span<double> upM,
        double mostEarthworkM)
      : These(these),
        At(at),
        UpM(upM),
        MostEarthworkM(mostEarthworkM),
        Buckets(BucketOver(these, mostEarthworkM)),
        Structures(these.size(), 0u) {
    if (these.empty() || at.size() != upM.size()) {
      Current = Phase::Done;
      return;
    }
    Result.BucketMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
    Result.DecidedBy.reserve(at.size());
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Buckets.HeapBytes() + Structures.capacity() * sizeof(uint8_t) +
           Profiles.capacity() * sizeof(NearestProfile) +
           Result.Refused.capacity() * sizeof(uint8_t) +
           Result.DecidedBy.capacity() * sizeof(uint32_t) +
           Result.Inside.capacity() * sizeof(EarthworkPointClaim);
  }
};

EarthworkPressJob::EarthworkPressJob(std::span<const EarthworkStamp> these,
                                     std::span<const EastNorth> at,
                                     std::span<double> upM,
                                     double mostEarthworkM)
    : State_(std::make_unique<State>(these, at, upM, mostEarthworkM)) {}

EarthworkPressJob::~EarthworkPressJob() = default;
EarthworkPressJob::EarthworkPressJob(EarthworkPressJob &&) noexcept = default;
EarthworkPressJob &EarthworkPressJob::operator=(EarthworkPressJob &&) noexcept = default;

bool EarthworkPressJob::Advance(size_t pointsMost) {
  State &state = *State_;
  if (state.Current == State::Phase::Done) { return true; }
  if (pointsMost == 0) { return false; }
  const size_t end = state.Next + std::min(pointsMost, state.At.size() - state.Next);
  const auto began = std::chrono::steady_clock::now();
  if (state.Current == State::Phase::Reject) {
    for (; state.Next < end; ++state.Next) {
      RejectAt(state.These,
               state.Buckets,
               state.At,
               state.UpM,
               state.Structures,
               state.MostEarthworkM,
               state.Next);
    }
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    state.Result.RejectMs += elapsed;
    state.Result.LongestRejectMs = std::max(state.Result.LongestRejectMs, elapsed);
    if (state.Next < state.At.size()) { return false; }
    for (const uint8_t rejected : state.Structures) { state.Result.Structures += rejected; }
    state.Next = 0;
    state.Current = State::Phase::InitializeDecisions;
    return false;
  }
  if (state.Current == State::Phase::InitializeDecisions) {
    state.Result.DecidedBy.insert(state.Result.DecidedBy.end(), end - state.Next, kNoStamp);
    state.Result.LongestInitializeMs =
        std::max(state.Result.LongestInitializeMs,
                 std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                     .count());
    state.Next = end;
    if (state.Next < state.At.size()) { return false; }
    state.Next = 0;
    state.Current = State::Phase::Apply;
    return false;
  }
  for (; state.Next < end; ++state.Next) {
    ApplyAt(state.These,
            state.Buckets,
            state.At,
            state.UpM,
            state.Structures,
            state.MostEarthworkM,
            state.Result,
            state.Next,
            state.Profiles);
  }
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  state.Result.ApplyMs += elapsed;
  state.Result.LongestApplyMs = std::max(state.Result.LongestApplyMs, elapsed);
  if (state.Next < state.At.size()) { return false; }
  state.Result.Refused = std::move(state.Structures);
  state.Current = State::Phase::Done;
  return true;
}

EarthworkPressResult EarthworkPressJob::Take() noexcept {
  assert(State_ && State_->Current == State::Phase::Done);
  return std::move(State_->Result);
}

size_t EarthworkPressJob::HeapBytes() const noexcept {
  return State_ ? State_->HeapBytes() : 0;
}

EarthworkMetrics MeasureEarthworkEffect(std::span<const EarthworkStamp> these,
                                        const EarthworkPressResult &pressed,
                                        EarthworkKind kind,
                                        std::span<const EastNorth> at,
                                        EarthworkHeightView heights) {
  const std::span<const double> upM = heights.WrittenM;
  const std::span<const double> wasM = heights.WasM;
  EarthworkMetrics told;
  std::vector<uint8_t> reached(these.size(), 0u);
  for (const EarthworkPointClaim &claim : pressed.Inside) {
    const EarthworkStamp &held = these[claim.Stamp];
    if (held.Kind != kind || pressed.Refused[claim.Stamp] != 0u) { continue; }
    reached[claim.Stamp] = 1u;
    const uint32_t by = pressed.DecidedBy[claim.Point];
    if (by == kHeldStamp) { continue; }
    ++told.Nodes;
    if (by != kNoStamp && by != claim.Stamp) {
      ++told.Contested;
      continue;
    }
    const double wanted = held.WantsAt(at[claim.Point]);
    told.AboveM = std::max(told.AboveM, upM[claim.Point] - wanted);
    told.WasAboveM = std::max(told.WasAboveM, wasM[claim.Point] - wanted);
    if (held.Fills) {
      told.BelowM = std::max(told.BelowM, wanted - upM[claim.Point]);
      told.WasBelowM = std::max(told.WasBelowM, wanted - wasM[claim.Point]);
    } else {
      told.UnfilledM = std::max(told.UnfilledM, wanted - upM[claim.Point]);
    }
  }
  for (size_t one = 0; one < these.size(); ++one) {
    if (these[one].Kind != kind || pressed.Refused[one] != 0u) { continue; }
    if (reached[one] != 0u) {
      ++told.Stamps;
    } else {
      ++told.Unreached;
    }
  }
  return told;
}

}
