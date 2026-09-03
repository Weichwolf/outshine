#include <span>
#include "Water.h"
#include <array>
#include <cstddef>
#include <optional>
#include <cstdint>

namespace outshine::Generators {

std::span<const char *const> Water::NoteNames() const noexcept {
  static constexpr std::array<const char *const, kNotes> kNames = {
      "waterSurfaces", "waterUntested", "levelBelowGround", "deepestM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return {kNames.data(), kNames.size()};
}

WaterDepth Water::DepthAt(const Ground &ground, EastNorth at) noexcept {
  const double eastM = at.EastM;
  const double northM = at.NorthM;
  const FeatureField &features = ground.Features();
  bool wet = false;
  float levelAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Water ||
        !FeatureField::Boxed(f, {.EastM = eastM, .NorthM = northM})) {
      continue;
    }
    const float atAslM = f.Top.AslM().value_or(0.0f);
    if (wet && atAslM <= levelAslM) { continue; }
    if (!features.Contains(f, {.EastM = eastM, .NorthM = northM})) { continue; }
    wet = true;
    levelAslM = atAslM;
  }
  if (!wet) { return WaterDepth::Dry(); }
  return WaterDepth::Between(static_cast<double>(levelAslM),
                             ground.HeightAslM({.EastM = eastM, .NorthM = northM}));
}

void Water::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Water) { continue; }
    yield.Count(Surfaces);
    double e = 0.0;
    double n = 0.0;
    double count = 0.0;
    for (const FeatureField::Ring &r : features.Rings(f)) {
      for (const FeatureField::Vertex &v : features.Vertices(r)) {
        e += static_cast<double>(v.Em);
        n += static_cast<double>(v.Nm);
        count += 1.0;
      }
    }
    if (count <= 0.0 || !features.Contains(f, {.EastM = e / count, .NorthM = n / count})) {
      yield.Count(Untested);
      continue;
    }
    const float levelAslM = f.Top.AslM().value_or(0.0f);
    const WaterDepth depth =
        WaterDepth::Between(static_cast<double>(levelAslM),
                            ground.HeightAslM({.EastM = e / count, .NorthM = n / count}));
    if (const std::optional<double> m = depth.DepthM()) { yield.Raise(DeepestM, *m); }

    if (const std::optional<double> m = depth.DisagreementM()) {
      yield.Raise(LevelBelowGround, *m);
    }
  }
}

} // namespace outshine::Generators
