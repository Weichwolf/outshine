#ifndef OUTSHINE_WORLD_WEATHER_CLOUDMOMENTS_H
#define OUTSHINE_WORLD_WEATHER_CLOUDMOMENTS_H

#include "CloudLayer.h"

namespace outshine {

struct CloudDepthMoments {
  double Weight = 0.0;
  double WeightedDistanceM = 0.0;
};

[[nodiscard]] CloudDepthMoments TraceCloudMoments(const CloudLayer &layer,
                                                  const Vec3 &originM,
                                                  const Vec3 &direction,
                                                  const CloudTrace &trace = {}) noexcept;

}
#endif
