#include <span>
#include "Forest.h"

#include <array>
#include <algorithm>
#include <optional>
#include <cstdint>
#include <cstddef>
#include <type_traits>

#include <numbers>
#include <cmath>
#include <utility>

#include "Geodesy.h"
#include "math/Units.h"

namespace outshine::Generators {

constexpr float kRootSix = 2.4494897f;

namespace {

constexpr uint64_t kMantissa24Mask = 0xFFFFFFu;
constexpr float kMantissa24Steps = 16777216.0f;
constexpr uint64_t kMantissa16Mask = 0xFFFFu;
constexpr float kMantissa16Steps = 65536.0f;
constexpr unsigned kSecondDrawShift = 16u;
constexpr unsigned kWoodyShift = 24u;
constexpr unsigned kYawShift = 48u;
constexpr size_t kStemBytes = 24;

constexpr uint64_t kStreamsPerCell = 4;

float Unit24(uint64_t bits) {
  return static_cast<float>(bits & kMantissa24Mask) * (1.0f / kMantissa24Steps);
}

float Unit16(uint64_t bits) {
  return static_cast<float>(bits & kMantissa16Mask) * (1.0f / kMantissa16Steps);
}

float SizeFactor(uint64_t bits, float sigma) {
  return 1.0f + sigma * (Unit16(bits) + Unit16(bits >> kSecondDrawShift) - 1.0f) * kRootSix;
}

} // namespace

Forest::Forest(std::span<const Stem> stems, std::span<const float> perM2ByRow, AlpineLimit limit)
    : PerM2_(perM2ByRow.begin(), perM2ByRow.end()), Limit_(std::move(limit)) {
  Held_ = stems.size() < kMostSpecies ? stems.size() : kMostSpecies;
  Refused_ = stems.size() - Held_;
  for (size_t at = 0; at < Held_; ++at) { Stems_[at] = stems[at]; }
}

std::span<const char *const> Forest::NoteNames() const noexcept {
  static constexpr std::array<const char *const, kNotes> kNames = {"noTemplate",
                                                                   "noSpecies",
                                                                   "zeroDensity",
                                                                   "densityDraw",
                                                                   "aboveTreeline",
                                                                   "tooSteep",
                                                                   "woodyDraw",
                                                                   "highestStandAslM"};
  static_assert(sizeof(Forest::Stem) == kStemBytes, "sizeof(Forest::Stem)");
  static_assert(std::is_trivially_copyable_v<Forest::Stem>, "a stem is copied per cell");
  static_assert(Forest::kSpeciesTableBytes == Forest::kMostSpecies * kStemBytes,
                "the species table is as many stems wide as the catalogue holds");

  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return {kNames.data(), kNames.size()};
}

Forest::Lattice Forest::Of(const Tile &region) {
  Lattice l;
  l.Cols = static_cast<int>(std::lround(region.SpanEm() / kCellM));
  l.Rows = static_cast<int>(std::lround(region.SpanNm() / kCellM));
  l.Cols = std::max(l.Cols, 1);
  l.Rows = std::max(l.Rows, 1);
  l.Em = region.SpanEm() / static_cast<double>(l.Cols);
  l.Nm = region.SpanNm() / static_cast<double>(l.Rows);
  return l;
}

Forest::Outcome Forest::Consider(const Ground &ground,
                                 const Lattice &lattice,
                                 Cell cell,
                                 Solid *out) const noexcept {
  const Tile &region = ground.Where();
  const uint64_t index = static_cast<uint64_t>(cell.J) * static_cast<uint64_t>(lattice.Cols) +
                         static_cast<uint64_t>(cell.I);
  const uint64_t place = region.Seed(index * kStreamsPerCell);
  const double eastM =
      (static_cast<double>(cell.I) + 0.25 + 0.5 * static_cast<double>(Unit24(place))) * lattice.Em;
  const double northM =
      (static_cast<double>(cell.J) + 0.25 + 0.5 * static_cast<double>(Unit24(place >> 24u))) *
      lattice.Nm;

  const std::optional<int> row = ground.CoverAt({.EastM = eastM, .NorthM = northM}).Row();
  if (!row || static_cast<size_t>(*row) >= PerM2_.size()) { return Outcome::NoTemplate; }
  const float perM2 = PerM2_[static_cast<size_t>(*row)];
  if (perM2 <= 0.0f) { return Outcome::ZeroDensity; }

  const uint64_t draw = region.Seed(index * kStreamsPerCell + 1);
  if (Unit24(draw) > static_cast<float>(static_cast<double>(perM2) * lattice.Em * lattice.Nm)) {
    return Outcome::DensityDraw;
  }

  const double aslM = ground.HeightAslM({.EastM = eastM, .NorthM = northM});

  const double latDeg = region.AnchorLat();
  const double jitterE = Wrap180(region.AnchorLon()) * kMPerDeg * std::cos(latDeg * kDeg2Rad);
  const double jitterN = latDeg * kMPerDeg;
  const double woody = Limit_.WoodyFraction(
      {.LongitudeDeg = region.AnchorLon(), .LatitudeDeg = latDeg, .HeightM = aslM},
      {.EastM = jitterE + eastM, .NorthM = jitterN + northM});
  double steep = 0.0;
  if (woody > 0.0 && static_cast<size_t>(*row) < ground.Table().Count()) {
    steep = Limit_.BareBySlope(
        ground.SlopeDeg({.EastM = eastM, .NorthM = northM}),
        static_cast<double>(ground.Table().At(static_cast<size_t>(*row)).SlopeMaxDeg));
  }
  if (woody <= 0.0) { return Outcome::AboveTreeline; }
  if (steep >= 1.0) { return Outcome::TooSteep; }

  if (static_cast<double>(Unit24(draw >> kWoodyShift)) >= woody * (1.0 - steep)) {
    return Outcome::WoodyDraw;
  }

  if (Held_ == 0) { return Outcome::NoSpecies; }
  const Stem &stem = Stems_[static_cast<size_t>(region.Seed(index * kStreamsPerCell + 3) % Held_)];
  const float size = SizeFactor(region.Seed(index * kStreamsPerCell + 2), stem.HeightSigma);
  out->Em = eastM;
  out->Nm = northM;
  out->BaseAslM = aslM;
  out->RadiusM = stem.TrunkRadiusM * size;
  out->HeightM = static_cast<float>(stem.HeightM * static_cast<double>(size));
  out->MassKg = stem.MassKg * size * size * size;
  out->YawRad = Unit16(place >> kYawShift) * 2.0f * std::numbers::pi_v<float>;
  out->Contact = stem.Contact;
  return Outcome::Placed;
}

void Forest::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const Lattice lattice = Of(ground.Where());
  for (int j = 0; j < lattice.Rows; j++) {
    for (int i = 0; i < lattice.Cols; i++) {
      Solid body;
      const Outcome why = Consider(ground, lattice, Cell{.I = i, .J = j}, &body);
      switch (why) {
        case Outcome::NoTemplate: yield.Count(NoTemplate); continue;
        case Outcome::NoSpecies: yield.Count(NoSpecies); continue;
        case Outcome::ZeroDensity: yield.Count(ZeroDensity); continue;
        case Outcome::DensityDraw: yield.Count(DensityDraw); continue;
        case Outcome::AboveTreeline: yield.Count(AboveTreeline); continue;
        case Outcome::TooSteep: yield.Count(TooSteep); continue;
        case Outcome::WoodyDraw: yield.Count(WoodyDraw); continue;
        case Outcome::Placed: break;
      }
      const Claim claim = yield.Place(body);

      if (claim.Why() == Claim::Outcome::Full) { return; }
      if (claim.Why() == Claim::Outcome::Placed) {
        yield.Raise({.Name = HighestStandAslM, .Value = body.BaseAslM});
      }
    }
  }
}

} // namespace outshine::Generators
