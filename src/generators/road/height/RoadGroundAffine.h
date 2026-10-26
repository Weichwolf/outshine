#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDAFFINE_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDAFFINE_H

#include "RoadGroundStrip.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>

namespace outshine::Generators {
inline constexpr uint32_t kRoadGroundAffineRevision = 1;
inline constexpr size_t kRoadGroundAffineRoundingRounds = 8;

struct RoadGroundAffine {
  RoadPortField Field;
  double MaximumPriorErrorM = 0;
  double MaximumNormalizedGrade = 0;
  size_t Observations = 0;
  size_t Rotations = 0;
  size_t GradeFrameTests = 0;
};

[[nodiscard]] std::expected<RoadGroundAffine, RoadGroundError> FitRoadGroundAffine(
    const RoadGroundField &input, RoadGroundBudget budget = {}, const std::stop_token &stop = {});
}
#endif
