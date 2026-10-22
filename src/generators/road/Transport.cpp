#include <span>
#include "Transport.h"
#include "FeatureField.h"
#include <array>
#include <cstddef>

namespace outshine::Generators {

std::span<const char *const> Transport::NoteNames() const noexcept {
  static constexpr std::array<const char *const, kNotes> kNames = {"ways", "widestWayM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return {kNames.data(), kNames.size()};
}

void Transport::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Way) { continue; }
    yield.Count(Ways);
    if (f.Form == FeatureForm::Ribbon) {
      yield.Raise({.Name = WidestM, .Value = 2.0 * static_cast<double>(f.HalfWidthM)});
    }
  }
}

}
