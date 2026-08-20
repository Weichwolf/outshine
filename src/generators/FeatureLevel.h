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

}
#endif
