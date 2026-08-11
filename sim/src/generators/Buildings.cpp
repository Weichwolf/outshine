#include "Buildings.h"

#include <cmath>

namespace outshine::Generators {

namespace {

/* [SET] kg/m3 of GROSS BUILT VOLUME, not of a material: a masonry house is mostly air, and what a
 * substitute body needs is the mass the whole prism carries. 300 is the middle of the 250...350 that
 * European residential construction lands on once floors, walls and roof are spread over the volume
 * they enclose. It is a placeholder for a per-material sum and is deleted the day one exists. */
constexpr double kBuiltDensityKgPerM3 = 300.0;

} // namespace

Buildings::Buildings(int coverRow, ContactMaterial contact)
    : Sieve_{(int32_t)coverRow, true}, Contact_(contact) {}

Span<const char *const> Buildings::NoteNames() const noexcept {
  static const char *const kNames[kNotes] = {"footprints", "roofless", "highestRoofAglM"};
  return Span<const char *const>(kNames, kNotes);
}

/* Nothing is claimed, so this counts what it saw: a region whose outlines all vanished and a region
 * that has none are otherwise the same empty line. */
void Buildings::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.CoverRow != Sieve_.CoverRow) continue;
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
    if (!features.Passes(f, Sieve_) || !FeatureField::Boxed(f, eastM, northM)) continue;
    float topAslM = 0.0f;
    (void)f.Top.TryAslM(&topAslM);
    if (highest && topAslM <= highestAslM) continue;
    if (!features.Contains(f, eastM, northM)) continue;
    highest = &f;
    highestAslM = topAslM;
  }
  return highest;
}

/* The prism the point stands under, as the body it would be. Its cylinder is INSCRIBED in the
 * outline's own box: a substitute larger than the thing it substitutes claims ground the building
 * does not stand on, and a terrace is where that shows. */
bool Buildings::At(const Ground &ground, double eastM, double northM, Body *out) const noexcept {
  const FeatureField::Feature *f = Over(ground, eastM, northM);
  if (!f) return false;
  float topAslM = 0.0f;
  if (!f->Top.TryAslM(&topAslM)) return false;

  const double e = 0.5 * ((double)f->MinEm + (double)f->MaxEm);
  const double n = 0.5 * ((double)f->MinNm + (double)f->MaxNm);
  const double baseAslM = ground.HeightAslM(e, n);
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

} // namespace outshine::Generators
