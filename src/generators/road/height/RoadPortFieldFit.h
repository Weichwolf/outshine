#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDFIT_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDFIT_H

#include "HeightProgram.h"
#include "RoadPortField.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>

namespace outshine::Generators {
struct RoadPortPrior {
  uint32_t Patch = 0;
  EastNorth At;
  double HeightM = 0;
};

struct RoadPortFace {
  uint32_t Patch = 0;
  std::array<EastNorth, 3> Triangle;
  std::array<double, 4> GradeFrame;
};

struct RoadPortFieldFitInput {
  std::span<const RoadPortField::Plane> Planes;
  std::span<const RoadPortField::Patch> Patches;
  std::span<const RoadPortPrior> Priors;
  std::span<const RoadPortFace> Faces;
  double MaximumPriorChangeM = 0;
};

struct RoadPortFieldFitWork {
  size_t Observations = 0;
  size_t Faces = 0;
  size_t GradientControls = 0;
  size_t Conditions = 0;
  size_t Nonzeros = 0;
};

struct RoadPortFieldFit {
  RoadPortField Field;
  RoadProfiles::HeightFit Solver;
  RoadPortFieldFitWork Work;
  double MaximumPriorErrorM = 0;
  double MaximumNormalizedGrade = 0;
};

[[nodiscard]] std::expected<RoadPortFieldFit, RoadProfiles::HeightFitError>
FitRoadPortField(const RoadPortFieldFitInput &input,
                 RoadProfiles::HeightFitBudget budget,
                 const std::stop_token &stop = {});
}
#endif
