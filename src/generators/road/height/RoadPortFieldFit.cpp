#include "RoadPortFieldFit.h"
#include "RoadPortFieldProgram.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
std::expected<RoadPortFieldFit, RoadProfiles::HeightFitError>
FitRoadPortField(const RoadPortFieldFitInput &input,
                 RoadProfiles::HeightFitBudget budget,
                 const std::stop_token &stop) {
  using Error = RoadProfiles::HeightFitError;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (budget.Variables == 0 || input.Planes.size() > (budget.Variables - 1) / 3 ||
      input.Priors.size() > budget.Conditions / 2 ||
      input.Faces.size() > budget.Conditions / kPortFitPolygonSides) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto prototype = RoadPortField::Make(input.Planes, input.Patches);
  if (!prototype) { return std::unexpected(Error::InvalidInput); }
  const auto program = BuildRoadPortFieldProgram(input, *prototype, budget, stop);
  if (!program) { return std::unexpected(program.error()); }
  auto solved = RoadProfiles::FitHeights(program->Program, budget, stop);
  if (!solved) { return std::unexpected(solved.error()); }
  std::vector<RoadPortField::Plane> planes(input.Planes.begin(), input.Planes.end());
  for (size_t at = 0; at < planes.size(); ++at) {
    planes[at].HeightM = solved->Values[3 * at] + program->DatumM;
    planes[at].Gradient = {.SlopeE = solved->Values[3 * at + 1],
                           .SlopeN = solved->Values[3 * at + 2]};
  }
  auto field = RoadPortField::Make(planes, input.Patches);
  if (!field) { return std::unexpected(Error::VerificationFailed); }
  double maximumError = 0;
  double maximumGrade = 0;
  for (const auto &prior : input.Priors) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto value = field->Evaluate(prior.Patch, prior.At);
    if (!value) { return std::unexpected(Error::VerificationFailed); }
    maximumError = std::max(maximumError, std::abs(value->HeightM - prior.HeightM));
  }
  for (const auto &face : input.Faces) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto bound = field->Bounds(face.Patch, face.Triangle, face.GradeFrame);
    if (!bound) { return std::unexpected(Error::VerificationFailed); }
    maximumGrade = std::max(maximumGrade, bound->NormalizedGradientMaximum);
  }
  constexpr double tolerance = 1e-7;
  if (maximumError > input.MaximumPriorChangeM + tolerance || maximumGrade > 1 + tolerance) {
    return std::unexpected(Error::VerificationFailed);
  }
  return RoadPortFieldFit{.Field = std::move(*field),
                          .Solver = std::move(*solved),
                          .Work = program->Work,
                          .MaximumPriorErrorM = maximumError,
                          .MaximumNormalizedGrade = maximumGrade};
}
}
