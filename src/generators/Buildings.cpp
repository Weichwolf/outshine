#include "Buildings.h"

#include <cmath>

namespace outshine::Generators {

namespace {

constexpr double kBuiltDensityKgPerM3 = 300.0;

}

Buildings::Buildings(ContactMaterial contact) : Contact_(contact) {}

Span<const char *const> Buildings::NoteNames() const noexcept {
  static constexpr const char *const kNames[kNotes] = {"footprints", "roofless", "highestRoofAglM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return Span<const char *const>(kNames, kNotes);
}

void Buildings::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure) { continue; }
    float topAslM = 0.0f;
    if (!f.Top.TryAslM(&topAslM)) {
      yield.Count(Roofless);
      continue;
    }
    yield.Count(Footprints);
    const double e = 0.5 * (static_cast<double>(f.MinEm) + static_cast<double>(f.MaxEm));
    const double n = 0.5 * (static_cast<double>(f.MinNm) + static_cast<double>(f.MaxNm));
    const double standsAtM = ground.HeightAslM(e, n);
    yield.Raise(HighestRoofAglM, static_cast<double>(topAslM) - standsAtM);

    Body body;
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

uint32_t Buildings::Proposes(double areaM2) const noexcept {
  constexpr double kDensestM2PerBuilding = 400.0;
  const double most = areaM2 / kDensestM2PerBuilding;
  return static_cast<uint32_t>(most + 1.0);
}

const FeatureField::Feature *
Buildings::Over(const Ground &ground, double eastM, double northM) const noexcept {
  const FeatureField &features = ground.Features();
  const FeatureField::Feature *highest = nullptr;
  float highestAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure || !FeatureField::Boxed(f, eastM, northM)) { continue; }
    float topAslM = 0.0f;
    (void)f.Top.TryAslM(&topAslM);
    if ((highest != nullptr) && topAslM <= highestAslM) { continue; }
    if (!features.Contains(f, eastM, northM)) { continue; }
    highest = &f;
    highestAslM = topAslM;
  }
  return highest;
}

bool Buildings::At(const Ground &ground, double eastM, double northM, Body *out) const noexcept {
  const FeatureField::Feature *f = Over(ground, eastM, northM);
  if (f == nullptr) { return false; }
  float topAslM = 0.0f;
  float baseM = 0.0f;
  if (!f->Top.TryAslM(&topAslM) || !f->Base.TryAslM(&baseM)) { return false; }

  const double e = 0.5 * (static_cast<double>(f->MinEm) + static_cast<double>(f->MaxEm));
  const double n = 0.5 * (static_cast<double>(f->MinNm) + static_cast<double>(f->MaxNm));
  const auto baseAslM = static_cast<double>(baseM);
  const double halfE = 0.5 * (static_cast<double>(f->MaxEm) - static_cast<double>(f->MinEm));
  const double halfN = 0.5 * (static_cast<double>(f->MaxNm) - static_cast<double>(f->MinNm));
  const double radiusM = halfE < halfN ? halfE : halfN;
  const double heightM = static_cast<double>(topAslM) - baseAslM;
  out->Em = e;
  out->Nm = n;
  out->BaseAslM = baseAslM;
  out->RadiusM = static_cast<float>(radiusM);
  out->HeightM = static_cast<float>(heightM > 0.0 ? heightM : 0.0);
  out->MassKg = static_cast<float>(4.0 * halfE * halfN * static_cast<double>(out->HeightM) *
                                   kBuiltDensityKgPerM3);
  out->YawRad = 0.0f;
  out->Contact = Contact_;
  return true;
}

} // namespace outshine::Generators
