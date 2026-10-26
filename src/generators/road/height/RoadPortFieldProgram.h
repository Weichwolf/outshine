#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDPROGRAM_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDPROGRAM_H

#include "RoadPortFieldFit.h"
#include <cstddef>
#include <expected>
#include <stop_token>

namespace outshine::Generators {
inline constexpr size_t kPortFitPolygonSides = 16;

struct RoadPortFieldProgram {
  RoadProfiles::HeightProgram Program;
  RoadPortFieldFitWork Work;
  double DatumM = 0;
};

[[nodiscard]] std::expected<RoadPortFieldProgram, RoadProfiles::HeightFitError>
BuildRoadPortFieldProgram(const RoadPortFieldFitInput &input,
                          const RoadPortField &prototype,
                          RoadProfiles::HeightFitBudget budget,
                          const std::stop_token &stop);
}
#endif
