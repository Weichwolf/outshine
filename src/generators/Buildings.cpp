#include <span>
#include "Buildings.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <cstdint>

namespace outshine::Generators {

namespace {}

Buildings::Buildings(ContactMaterial contact) : Contact_(contact) {}

std::span<const char *const> Buildings::NoteNames() const noexcept {
  static constexpr std::array<const char *const, kNotes> kNames = {
      "footprints", "roofless", "highestRoofAglM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return {kNames.data(), kNames.size()};
}

void Buildings::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure) { continue; }
    const std::optional<float> roof = f.Top.AslM();
    if (!roof) {
      yield.Count(Roofless);
      continue;
    }
    const float topAslM = *roof;
    yield.Count(Footprints);
    const double e = 0.5 * (static_cast<double>(f.MinEm) + static_cast<double>(f.MaxEm));
    const double n = 0.5 * (static_cast<double>(f.MinNm) + static_cast<double>(f.MaxNm));
    const double standsAtM = ground.HeightAslM({.EastM = e, .NorthM = n});
    yield.Raise(HighestRoofAglM, static_cast<double>(topAslM) - standsAtM);

    Solid body;
    body.Em = e;
    body.Nm = n;
    body.BaseAslM = standsAtM;
    const double acrossEm = static_cast<double>(f.MaxEm) - static_cast<double>(f.MinEm);
    const double acrossNm = static_cast<double>(f.MaxNm) - static_cast<double>(f.MinNm);
    body.RadiusM = static_cast<float>(0.5 * std::sqrt(acrossEm * acrossEm + acrossNm * acrossNm));
    body.HeightM = static_cast<float>(static_cast<double>(topAslM) - standsAtM);
    body.MassKg = 0.0f;
    body.YawRad = 0.0f;
    body.Contact = Contact_;
    const Claim claim = yield.Place(body);
    if (claim.Why() == Claim::Outcome::Full) { return; }
  }
}

const FeatureField::Feature *Buildings::Over(const Ground &ground, EastNorth at) noexcept {
  const double eastM = at.EastM;
  const double northM = at.NorthM;
  const FeatureField &features = ground.Features();
  const FeatureField::Feature *highest = nullptr;
  float highestAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure ||
        !FeatureField::Boxed(f, {.EastM = eastM, .NorthM = northM})) {
      continue;
    }
    const float topAslM = f.Top.AslM().value_or(0.0f);
    if ((highest != nullptr) && topAslM <= highestAslM) { continue; }
    if (!features.Contains(f, {.EastM = eastM, .NorthM = northM})) { continue; }
    highest = &f;
    highestAslM = topAslM;
  }
  return highest;
}

} // namespace outshine::Generators
