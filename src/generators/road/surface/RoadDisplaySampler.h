#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_ROADDISPLAYSAMPLER_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_ROADDISPLAYSAMPLER_H

#include "ReferenceLine.h"
#include "Ribbon.h"
#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct RoadDisplayLimits {
  size_t StationsMost = 0;
  size_t PoseQueriesMost = 0;
};

struct RoadDisplayPlan {
  std::vector<double> StationsM;
  double PositionBoundM = 0;
  double SourceTruncationM = 0;
  size_t PoseQueries = 0;
};

// Upper positions only; ports, normals, closed bodies and full FP error remain separate.
[[nodiscard]] std::expected<RoadDisplayPlan, std::string_view> SampleRoadDisplay(
    const ReferenceLine &line, Section section, double errorM, RoadDisplayLimits limits);
}

#endif
