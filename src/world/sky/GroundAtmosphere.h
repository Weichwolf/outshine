#ifndef OUTSHINE_WORLD_SKY_GROUNDATMOSPHERE_H
#define OUTSHINE_WORLD_SKY_GROUNDATMOSPHERE_H

#include "Atmosphere.h"
#include <cstddef>

namespace outshine {
struct GroundLight {
  Vec3f SunTransmittance;
  Vec3f SkyIrradiance;
};

class GroundAtmosphere {
public:
  struct Work {
    size_t SkyQueries = 0;
    size_t ScatterEvaluations = 0;
  };

  [[nodiscard]] GroundLight
  Evaluate(const Medium &medium, double cosSunZenith, Work *work = nullptr) const;

  [[nodiscard]] size_t Integrations() const noexcept { return Integrations_; }

private:
  mutable float CachedCosine_ = -2.0f;
  mutable Medium CachedMedium_{};
  mutable GroundLight CachedLight_{};
  mutable size_t Integrations_ = 0;
};
}
#endif
