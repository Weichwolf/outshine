#include "Infrastructure.h"

namespace outshine::Generators {

Span<const char *const> Infrastructure::NoteNames() const noexcept {
  static const char *const kNames[kNotes] = {"ways", "widestWayM"};
  return Span<const char *const>(kNames, kNotes);
}

std::optional<Infrastructure::Made> Infrastructure::MadeAt(const Ground &ground, double eastM,
                                                           double northM) const noexcept {
  const FeatureField &features = ground.Features();
  std::optional<Made> made;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way || !FeatureField::Boxed(f, eastM, northM)) continue;
    /* An area way has no declared width — what it covers is its own ring, and reporting a width for
     * it would be inventing one. */
    const float widthM = f.Form == FeatureForm::Ribbon ? 2.0f * f.HalfWidthM : 0.0f;
    if (made && widthM <= made->WidthM) continue;
    if (!features.Contains(f, eastM, northM)) continue;
    made = Made{f.CoverRow, widthM, ground.HeightAslM(eastM, northM)};
  }
  return made;
}

/* Nothing is claimed, so this counts what it saw: a region whose ways all fell outside and a region
 * that has none are otherwise the same empty line. */
void Infrastructure::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way) continue;
    yield.Count(Ways);
    if (f.Form == FeatureForm::Ribbon) yield.Raise(WidestM, 2.0 * (double)f.HalfWidthM);
  }
}

uint32_t Infrastructure::Proposes(double) const noexcept { return 0; }

bool Infrastructure::At(const Ground &, double, double, Body *) const noexcept { return false; }

} // namespace outshine::Generators
