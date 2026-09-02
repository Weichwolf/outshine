#ifndef OUTSHINE_GENERATORS_BASE_FEATURELEVEL_H
#define OUTSHINE_GENERATORS_BASE_FEATURELEVEL_H

#include <optional>

namespace outshine::Generators {

class FeatureLevel {
public:
  static FeatureLevel None() { return {}; }

  static FeatureLevel At(float aslM) {
    FeatureLevel level;
    level.AslM_ = aslM;
    level.Has_ = true;
    return level;
  }

  [[nodiscard]] std::optional<float> AslM() const {
    if (!Has_) { return std::nullopt; }
    return AslM_;
  }

private:
  float AslM_ = 0.0f;
  bool Has_ = false;
};

} // namespace outshine::Generators
#endif
