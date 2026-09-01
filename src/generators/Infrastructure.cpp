#include "Infrastructure.h"

namespace outshine::Generators {

Span<const char *const> Infrastructure::NoteNames() const noexcept {
  static constexpr const char *const kNames[kNotes] = {"ways", "widestWayM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return Span<const char *const>(kNames, kNotes);
}

std::optional<Infrastructure::Made>
Infrastructure::MadeAt(const Ground &ground, double eastM, double northM) const noexcept {
  const FeatureField &features = ground.Features();
  std::optional<Made> made;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way || !FeatureField::Boxed(f, eastM, northM)) { continue; }

    const float widthM = f.Form == FeatureForm::Ribbon ? 2.0f * f.HalfWidthM : 0.0f;
    if (made && widthM <= made->WidthM) { continue; }
    if (!features.Contains(f, eastM, northM)) { continue; }
    made = Made{
        .CoverRow = f.CoverRow, .WidthM = widthM, .SurfaceAslM = ground.HeightAslM(eastM, northM)};
  }
  return made;
}

void Infrastructure::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way) { continue; }
    yield.Count(Ways);
    if (f.Form == FeatureForm::Ribbon) {
      yield.Raise(WidestM, 2.0 * static_cast<double>(f.HalfWidthM));
    }
  }
}

uint32_t Infrastructure::Proposes(double) const noexcept {
  return 0;
}

bool Infrastructure::At(const Ground &, double, double, Body *) const noexcept {
  return false;
}

} // namespace outshine::Generators
