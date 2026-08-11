#include "Water.h"

namespace outshine::Generators {

Water::Water(int coverRow) : Sieve_{(int32_t)coverRow, true} {}

Span<const char *const> Water::NoteNames() const noexcept {
  static const char *const kNames[kNotes] = {"waterSurfaces", "waterUntested", "levelBelowGround",
                                             "deepestM"};
  return Span<const char *const>(kNames, kNotes);
}

/* The HIGHEST outline covering the point, so a basin inside a lake answers with its own level. */
WaterDepth Water::DepthAt(const Ground &ground, double eastM, double northM) const noexcept {
  const FeatureField &features = ground.Features();
  bool wet = false;
  float levelAslM = 0.0f;
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (!features.Passes(f, Sieve_) || !FeatureField::Boxed(f, eastM, northM)) continue;
    float atAslM = 0.0f;
    (void)f.Top.TryAslM(&atAslM);
    if (wet && atAslM <= levelAslM) continue;
    if (!features.Contains(f, eastM, northM)) continue;
    wet = true;
    levelAslM = atAslM;
  }
  if (!wet) return WaterDepth::Dry();
  return WaterDepth::Between((double)levelAslM, ground.HeightAslM(eastM, northM));
}

/* ONE SAMPLE PER OUTLINE, at its own vertex centroid, and only where that point is INSIDE the ring:
 * a river bends, so its box centre is as often bank as water and a count taken there would measure
 * the shape of the bend. What is not testable that way is counted rather than dropped — the three
 * counts partition the outlines of the region. A count over the interior would be a raster, and the
 * question it answers is a point query's (Water::DepthAt), not a region's. */
void Water::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const FeatureField &features = ground.Features();
  for (size_t i = 0; i < features.Count(); i++) {
    const FeatureField::Feature &f = features.At(i);
    if (!features.Passes(f, Sieve_)) continue;
    yield.Count(Surfaces);
    double e = 0.0, n = 0.0, count = 0.0;
    for (const FeatureField::Ring &r : features.Rings(f))
      for (const FeatureField::Vertex &v : features.Vertices(r)) {
        e += (double)v.Em;
        n += (double)v.Nm;
        count += 1.0;
      }
    if (count <= 0.0 || !features.Contains(f, e / count, n / count)) {
      yield.Count(Untested);
      continue;
    }
    float levelAslM = 0.0f;
    (void)f.Top.TryAslM(&levelAslM);
    const WaterDepth depth =
        WaterDepth::Between((double)levelAslM, ground.HeightAslM(e / count, n / count));
    double m = 0.0;
    if (depth.TryDepthM(&m)) yield.Raise(DeepestM, m);
    /* Raised and not counted: how far the two models are apart is the number that says whether an
     * outline is off by a bank or by a cliff, and a bare count cannot. */
    if (depth.TryDisagreementM(&m)) yield.Raise(LevelBelowGround, m);
  }
}

uint32_t Water::Proposes(double) const noexcept { return 0; }

bool Water::At(const Ground &, double, double, Body *) const noexcept { return false; }

} // namespace outshine::Generators
