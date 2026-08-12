/* A HEIGHT A FEATURE CARRIES OF ITS OWN, on the DEM's own datum. A roof, a floor and a water level
 * are the same statement about a vector outline — this is where the thing stops being ground — and
 * they arrive from the core: one off an OSM tag, one off the ring's lowest corner, one off the shore.
 *
 * MOST FEATURES HAVE NONE, so absence is a state and not a low number: a wood, a meadow and a street
 * ARE the ground, and a metre lying beside a flag is a metre that gets read without it. That is also
 * what makes a way different from a house — a house has one base, a way is graded onto the terrain
 * and its height is whatever the ground answers at the point asked for. */
#ifndef FEATURELEVEL_H
#define FEATURELEVEL_H

namespace outshine::Generators {

class FeatureLevel {
public:
  static FeatureLevel None() { return FeatureLevel(); }
  static FeatureLevel At(float aslM) {
    FeatureLevel level;
    level.AslM_ = aslM;
    level.Has_ = true;
    return level;
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
