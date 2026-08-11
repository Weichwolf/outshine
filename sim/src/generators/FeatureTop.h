/* THE HEIGHT OF A FEATURE'S OWN UPPER SURFACE, on the DEM's own datum. A roof and a water level are
 * the same statement about a vector outline — this is where the thing stops being ground — and both
 * arrive from the core, one off an OSM tag, one off the shore.
 *
 * MOST FEATURES HAVE NONE, so absence is a state and not a low number: a wood, a meadow and a street
 * are the ground, and a metre lying beside a flag is a metre that gets read without it. */
#ifndef FEATURETOP_H
#define FEATURETOP_H

namespace outshine::Generators {

class FeatureTop {
public:
  static FeatureTop None() { return FeatureTop(); }
  static FeatureTop At(float aslM) {
    FeatureTop t;
    t.AslM_ = aslM;
    t.Has_ = true;
    return t;
  }

  [[nodiscard]] bool TryAslM(float *out) const {
    if (!Has_) return false;
    *out = AslM_;
    return true;
  }

private:
  float AslM_ = 0.0f;
  bool Has_ = false;
};

} // namespace outshine::Generators
#endif
