#ifndef OUTSHINE_WORLD_WEATHER_CLOUDINTEGRATION_H
#define OUTSHINE_WORLD_WEATHER_CLOUDINTEGRATION_H

#include "CloudLayer.h"

#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace outshine {

struct CloudCell {
  double BeginM = 0.0;
  double LengthM = 0.0;
  double OpticalDepth = 0.0;
};

template <typename Visit>
void VisitCloudCells(const CloudLayer &layer,
                     const Vec3 &originM,
                     const Vec3 &direction,
                     const CloudTrace &trace,
                     Visit visit) noexcept {
  static_assert(std::is_nothrow_invocable_v<Visit, const CloudCell &>);
  if (layer.Cover <= 0.0) { return; }
  for (const CloudSegment &segment : CloudShell(layer, originM, direction)) {
    const double endM = std::min(segment.EndM, trace.MaximumM);
    if (endM <= segment.BeginM) { continue; }
    const double stepM = (endM - segment.BeginM) / static_cast<double>(kCloudIntegrationSamples);
    for (size_t sample = 0; sample < kCloudIntegrationSamples; ++sample) {
      const double beginM = segment.BeginM + static_cast<double>(sample) * stepM;
      const double distanceM = segment.BeginM + (static_cast<double>(sample) + 0.5) * stepM;
      visit(CloudCell{.BeginM = beginM,
                      .LengthM = stepM,
                      .OpticalDepth =
                          CloudExtinction(layer, originM + direction * distanceM, trace.Seconds) *
                          stepM});
    }
  }
}

}
#endif
