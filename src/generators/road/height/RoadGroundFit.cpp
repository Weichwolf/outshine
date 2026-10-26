#include "RoadGroundFit.h"
#include "RoadPortFieldProgram.h"
#include <cmath>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine::Generators {
namespace {
using Error = RoadProfiles::HeightFitError;
constexpr uint32_t kSelectionRevision = 1;
constexpr double kMaximumPriorChangeM = 1000;

Error AffineError(RoadGroundError error) noexcept {
  switch (error) {
    case RoadGroundError::Canceled: return Error::Canceled;
    case RoadGroundError::CapacityExceeded: return Error::CapacityExceeded;
    default: return Error::InvalidInput;
  }
}

TransportInputReference SelectionInput(TransportInputReference procedure) {
  procedure.ItemId = "affine-first/" + std::to_string(kSelectionRevision) + "/affine-" +
                     std::to_string(kRoadGroundAffineRevision) + "/" + procedure.ItemId;
  return procedure;
}
}

const RoadPortField &RoadGroundFit::Field() const noexcept {
  return std::visit([](const auto &fit) -> const RoadPortField & { return fit.Field; }, Value);
}

std::expected<RoadGroundFit, RoadProfiles::HeightFitError>
FitRoadGround(const RoadGroundField &input,
              double maximumPriorChangeM,
              RoadGroundBudget groundBudget,
              RoadProfiles::HeightFitBudget smoothBudget,
              const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!std::isfinite(maximumPriorChangeM) || maximumPriorChangeM <= 0 ||
      maximumPriorChangeM > kMaximumPriorChangeM) {
    return std::unexpected(Error::InvalidInput);
  }
  auto affine = FitRoadGroundAffine(input, groundBudget, stop);
  if (!affine) { return std::unexpected(AffineError(affine.error())); }
  const auto fitInput = input.FitInput(maximumPriorChangeM);
  if (affine->MaximumPriorErrorM <= maximumPriorChangeM && affine->MaximumNormalizedGrade <= 1) {
    auto procedure = BindRoadGroundAffineProcedure(fitInput, stop);
    if (!procedure) { return std::unexpected(procedure.error()); }
    return RoadGroundFit{.Value = std::move(*affine),
                         .RecipeInput = SelectionInput(std::move(procedure->Input))};
  }
  auto smooth = FitRoadPortField(fitInput, smoothBudget, stop);
  if (!smooth) { return std::unexpected(smooth.error()); }
  if (smooth->MaximumPriorErrorM > maximumPriorChangeM || smooth->MaximumNormalizedGrade > 1) {
    return std::unexpected(Error::VerificationFailed);
  }
  auto reference = SelectionInput(smooth->RecipeInput);
  return RoadGroundFit{.Value = std::move(*smooth), .RecipeInput = std::move(reference)};
}
}
