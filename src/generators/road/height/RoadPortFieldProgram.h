#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDPROGRAM_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDPROGRAM_H

#include "RoadPortFieldFit.h"
#include <cstddef>
#include <expected>
#include <stop_token>

namespace outshine::Generators {
inline constexpr size_t kPortFitPolygonSides = 16;
inline constexpr double kPortFitVerificationTolerance = 1e-7;
inline constexpr uint32_t kPortFitProcedureRevision = 1;

struct RoadPortProcedure {
  TransportInputReference Input;
  size_t Bytes = 0;
};

[[nodiscard]] std::expected<RoadPortProcedure, RoadProfiles::HeightFitError>
BindRoadPortProcedure(const RoadPortFieldFitInput &input, const std::stop_token &stop);

[[nodiscard]] std::expected<RoadPortProcedure, RoadProfiles::HeightFitError>
BindRoadGroundAffineProcedure(const RoadPortFieldFitInput &input, const std::stop_token &stop);

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
