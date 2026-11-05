#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_ROADDISPLAYSAMPLER_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_ROADDISPLAYSAMPLER_H

#include "ReferenceLine.h"
#include "RoadDisplayPolicy.h"
#include "Ribbon.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct RoadDisplayLimits {
  size_t StationsMost = 0;
  size_t PoseQueriesMost = 0;
};

struct RoadDisplayRequest {
  double ErrorM = 0;
  RoadDisplayLimits Limits;
  RoadDisplayPolicy Policy = RoadDisplayPolicy::UpperOnly;
};

struct RoadDisplayPlan {
  std::vector<double> StationsM;
  double PositionBoundM = 0;
  double SourceTruncationM = 0;
  double ContactDisplacementM = 0;
  size_t PoseQueries = 0;
};

struct RoadDisplayContacts {
  // Final source-frame Float32 positions, ordered along the curve at both ends.
  std::array<std::optional<std::array<Vec3f, kRibbonAcross>>, 2> EndRings;
};

// Upper position model includes storage/contact changes; normals, closed bodies and full FP remain
// separate.
[[nodiscard]] std::expected<RoadDisplayPlan, std::string_view>
SampleRoadDisplay(const ReferenceLine &line,
                  Section section,
                  double errorM,
                  RoadDisplayLimits limits,
                  const RoadDisplayContacts &contacts = {});
}

#endif
