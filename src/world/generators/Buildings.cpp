#include "Buildings.h"

#include <cmath>

namespace outshine::Generators {

namespace {

constexpr double kBuiltDensityKgPerM3 = 300.0;

}

Buildings::Buildings(ContactMaterial contact) : Contact_(contact) {}

Span<const char *const> Buildings::NoteNames() const noexcept {
  static constexpr const char *const kNames[kNotes] = {"footprints", "roofless", "highestRoofAglM"};
  static_assert(EveryNoteNamed(kNames),
                "every Note carries a name and none of them is empty");
  return Span<const char *const>(kNames, kNotes);
}

void Buildings::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure) continue;
    float topAslM = 0.0f;
    if (!f.Top.TryAslM(&topAslM)) {
      yield.Count(Roofless);
      continue;
    }
    yield.Count(Footprints);
    const double e = 0.5 * ((double)f.MinEm + (double)f.MaxEm);
    const double n = 0.5 * ((double)f.MinNm + (double)f.MaxNm);
    yield.Raise(HighestRoofAglM, (double)topAslM - ground.HeightAslM(e, n));
  }
}

uint32_t Buildings::Proposes(double) const noexcept { return 0; }

const FeatureField::Feature *Buildings::Over(const Ground &ground, double eastM,
                                            double northM) const noexcept {
  const FeatureField &features = ground.Features();
  const FeatureField::Feature *highest = nullptr;
  float highestAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Structure || !FeatureField::Boxed(f, eastM, northM)) continue;
    float topAslM = 0.0f;
    (void)f.Top.TryAslM(&topAslM);
    if (highest && topAslM <= highestAslM) continue;
    if (!features.Contains(f, eastM, northM)) continue;
    highest = &f;
    highestAslM = topAslM;
  }
  return highest;
}

bool Buildings::At(const Ground &ground, double eastM, double northM, Body *out) const noexcept {
  const FeatureField::Feature *f = Over(ground, eastM, northM);
  if (!f) return false;
  float topAslM = 0.0f, baseM = 0.0f;
  if (!f->Top.TryAslM(&topAslM) || !f->Base.TryAslM(&baseM)) return false;

  const double e = 0.5 * ((double)f->MinEm + (double)f->MaxEm);
  const double n = 0.5 * ((double)f->MinNm + (double)f->MaxNm);
  const double baseAslM = (double)baseM;
  const double halfE = 0.5 * ((double)f->MaxEm - (double)f->MinEm);
  const double halfN = 0.5 * ((double)f->MaxNm - (double)f->MinNm);
  const double radiusM = halfE < halfN ? halfE : halfN;
  const double heightM = (double)topAslM - baseAslM;
  out->Em = e;
  out->Nm = n;
  out->BaseAslM = baseAslM;
  out->RadiusM = (float)radiusM;
  out->HeightM = (float)(heightM > 0.0 ? heightM : 0.0);
  out->MassKg = (float)(4.0 * halfE * halfN * (double)out->HeightM * kBuiltDensityKgPerM3);
  out->YawRad = 0.0f;
  out->Contact = Contact_;
  return true;
}

}
