#include "Water.h"

namespace outshine::Generators {

Span<const char *const> Water::NoteNames() const noexcept {
  static constexpr const char *const kNames[kNotes] = {
      "waterSurfaces", "waterUntested", "levelBelowGround", "deepestM"};
  static_assert(EveryNoteNamed(kNames), "every Note carries a name and none of them is empty");
  return Span<const char *const>(kNames, kNotes);
}

WaterDepth Water::DepthAt(const Ground &ground, double eastM, double northM) const noexcept {
  const FeatureField &features = ground.Features();
  bool wet = false;
  float levelAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Water || !FeatureField::Boxed(f, eastM, northM)) { continue; }
    float atAslM = 0.0f;
    (void)f.Top.TryAslM(&atAslM);
    if (wet && atAslM <= levelAslM) { continue; }
    if (!features.Contains(f, eastM, northM)) { continue; }
    wet = true;
    levelAslM = atAslM;
  }
  if (!wet) { return WaterDepth::Dry(); }
  return WaterDepth::Between(static_cast<double>(levelAslM), ground.HeightAslM(eastM, northM));
}

void Water::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (f.Kind != FeatureKind::Water) { continue; }
    yield.Count(Surfaces);
    double e = 0.0, n = 0.0, count = 0.0;
    for (const FeatureField::Ring &r : features.Rings(f)) {
      for (const FeatureField::Vertex &v : features.Vertices(r)) {
        e += static_cast<double>(v.Em);
        n += static_cast<double>(v.Nm);
        count += 1.0;
      }
    }
    if (count <= 0.0 || !features.Contains(f, e / count, n / count)) {
      yield.Count(Untested);
      continue;
    }
    float levelAslM = 0.0f;
    (void)f.Top.TryAslM(&levelAslM);
    const WaterDepth depth = WaterDepth::Between(static_cast<double>(levelAslM),
                                                 ground.HeightAslM(e / count, n / count));
    double m = 0.0;
    if (depth.TryDepthM(&m)) { yield.Raise(DeepestM, m); }

    if (depth.TryDisagreementM(&m)) { yield.Raise(LevelBelowGround, m); }
  }
}

uint32_t Water::Proposes(double) const noexcept {
  return 0;
}

bool Water::At(const Ground &, double, double, Body *) const noexcept {
  return false;
}

} // namespace outshine::Generators
