#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFIT_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFIT_H

#include "RoadGroundAffine.h"
#include "RoadPortFieldFit.h"
#include <expected>
#include <stop_token>
#include <variant>

namespace outshine::Generators {
struct RoadGroundFit {
  std::variant<RoadGroundAffine, RoadPortFieldFit> Value;
  TransportInputReference RecipeInput;

  [[nodiscard]] const RoadPortField &Field() const noexcept;
};

[[nodiscard]] std::expected<RoadGroundFit, RoadProfiles::HeightFitError>
FitRoadGround(const RoadGroundField &input,
              double maximumPriorChangeM,
              RoadGroundBudget groundBudget = {},
              RoadProfiles::HeightFitBudget smoothBudget = {},
              const std::stop_token &stop = {});
}
#endif
