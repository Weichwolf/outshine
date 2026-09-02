#include "Infrastructure.h"
#include <array>
#include <optional>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

Span<const char *const> Infrastructure::NoteNames() const noexcept {
  static constexpr std::array<const char *const, kNotes> kNames = {"ways", "widestWayM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return {kNames.data(), kNames.size()};
}

std::optional<Infrastructure::Made> Infrastructure::MadeAt(const Ground &ground,
                                                           EastNorth at) noexcept {
  const double eastM = at.EastM;
  const double northM = at.NorthM;
  const FeatureField &features = ground.Features();
  std::optional<Made> made;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way || !FeatureField::Boxed(f, {.EastM = eastM, .NorthM = northM})) {
      continue;
    }

    const float widthM = f.Form == FeatureForm::Ribbon ? 2.0f * f.HalfWidthM : 0.0f;
    if (made && widthM <= made->WidthM) { continue; }
    if (!features.Contains(f, {.EastM = eastM, .NorthM = northM})) { continue; }
    made = Made{.CoverRow = f.CoverRow,
                .WidthM = widthM,
                .SurfaceAslM = ground.HeightAslM({.EastM = eastM, .NorthM = northM})};
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

} // namespace outshine::Generators
