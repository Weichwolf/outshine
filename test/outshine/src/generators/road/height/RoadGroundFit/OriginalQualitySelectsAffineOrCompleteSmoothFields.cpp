#include "RoadGroundFit.h"
#include "RoadGroundMesh.h"
#include "TransportPlanCodec.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kLimitM = .1;

std::expected<RoadGroundField, RoadGroundError> Input(bool hill) {
  constexpr std::array<LongitudeLatitude, 3> axis{{{.LongitudeDeg = 8, .LatitudeDeg = 47},
                                                   {.LongitudeDeg = 8.00075, .LatitudeDeg = 47},
                                                   {.LongitudeDeg = 8.0015, .LatitudeDeg = 47}}};
  const auto strip = PrepareRoadGroundStrip(axis, 2, .1);
  if (!strip) { return std::unexpected(strip.error()); }
  std::vector<double> heights;
  for (const auto at : strip->TerrainQueries) {
    const double along = (at.LongitudeDeg - 8) / .0015;
    heights.push_back(12 + (hill ? 1 - std::abs(2 * along - 1) : 0));
  }
  return BindRoadGroundField(*strip, heights);
}

auto Binding(const RoadGroundFit &fit) {
  return TransportPlanInputBinding(std::span(&fit.RecipeInput, 1), 4096);
}

void CheckAffine(const RoadGroundField &input) {
  RoadProfiles::HeightFitBudget noSmooth;
  noSmooth.Variables = 0;
  const auto fit = FitRoadGround(input, kLimitM, {}, noSmooth);
  CHECK(fit && std::holds_alternative<RoadGroundAffine>(fit->Value),
        "a qualifying original affine terrain never enters the disabled smooth solver");
  if (!fit) { return; }
  CHECK(fit->Field().HasSinglePlane() && Binding(*fit),
        "the selected shared plane and actual procedure reference reach the consumer");
  auto budget = RoadGroundBudget{};
  budget.TerrainSamples *= 2;
  budget.ContainerBytes *= 2;
  const auto repeated = FitRoadGround(input, kLimitM, budget);
  CHECK(repeated && Binding(*repeated) == Binding(*fit),
        "admitted preparation and solver budgets do not enter selection identity");
  const auto quality = FitRoadGround(input, 2 * kLimitM);
  CHECK(quality && Binding(*quality) != Binding(*fit),
        "a changed original quality requirement receives a different selected recipe binding");
  if (repeated) {
    for (const auto &prior : input.Priors) {
      const auto a = fit->Field().Evaluate(prior.Patch, prior.At);
      const auto b = repeated->Field().Evaluate(prior.Patch, prior.At);
      CHECK(a && b && a->HeightM == b->HeightM && a->Gradient.SlopeE == b->Gradient.SlopeE &&
                a->Gradient.SlopeN == b->Gradient.SlopeN,
            "independent affine selection retains exact field values and the full tangent plane");
    }
  }
}

void CheckSmooth(const RoadGroundField &input) {
  const auto fit = FitRoadGround(input, kLimitM);
  CHECK(fit && std::holds_alternative<RoadPortFieldFit>(fit->Value),
        "a full-width non-affine terrain automatically requires the complete C1 fit");
  if (!fit) { return; }
  const auto &smooth = std::get<RoadPortFieldFit>(fit->Value);
  CHECK(smooth.MaximumPriorErrorM <= kLimitM && smooth.MaximumNormalizedGrade <= 1 &&
            !fit->Field().HasSinglePlane(),
        "selection retains the original prior and continuous grade requirements");
  const auto raw = FitRoadPortField(input.FitInput(kLimitM), {});
  CHECK(raw && raw->Solver.Values == smooth.Solver.Values,
        "the required smooth path uses the existing verified solver without a new objective");
  CHECK(Binding(*fit) &&
            Binding(*fit) != TransportPlanInputBinding(std::span(&smooth.RecipeInput, 1), 4096),
        "the selected recipe binds the affine-first policy as well as the actual smooth fit");
  RoadProfiles::HeightFitBudget noSmooth;
  noSmooth.Variables = 0;
  const auto capped = FitRoadGround(input, kLimitM, {}, noSmooth);
  CHECK(!capped && capped.error() == RoadProfiles::HeightFitError::CapacityExceeded,
        "insufficient C1 admission cannot publish the rejected affine control as a fallback");
  const auto impossible = FitRoadGround(input, 0);
  CHECK(!impossible, "an unsatisfied original prior requirement never becomes a selected field");
  RoadProfiles::HeightFitBudget truncated;
  truncated.Iterations = 1;
  const auto incomplete = FitRoadGround(input, kLimitM, {}, truncated);
  CHECK(!incomplete && incomplete.error() == RoadProfiles::HeightFitError::CapacityExceeded,
        "a truncated smooth solve cannot publish a feasible-looking canonical substitute");
}

void CheckMesh(const RoadGroundField &input) {
  const auto fit = FitRoadGround(input, kLimitM);
  CHECK(fit.has_value(), "the single-plane mesh countercheck starts from a qualified field");
  if (!fit) { return; }
  const auto mesh = BuildRoadGroundMesh(input, fit->Field());
  CHECK(
      mesh && mesh->Surface.Points.size() == input.Vertices.size() &&
          mesh->Surface.Faces == input.Triangles && mesh->Audit.NumericalQualityMet() &&
          mesh->Work.CoverageErrorM2 <= 1e-6,
      "a selected shared plane keeps the original faces with complete physics and coverage checks");
  const std::array<RoadPortField::Plane, 1> planes{
      {{.Origin = {}, .HeightM = 12, .Gradient = {.SlopeE = 1}}}};
  const std::vector<RoadPortField::Patch> patches(input.Patches.size());
  const auto steep = RoadPortField::Make(planes, patches);
  CHECK(steep.has_value(), "the deliberately steep single plane remains structurally valid");
  if (!steep) { return; }
  const auto rejected = BuildRoadGroundMesh(input, *steep);
  CHECK(!rejected && rejected.error() == RoadGroundError::InvalidGeometry,
        "the single-plane fast path rejects invalid finite grades without useless subdivision");
}

void CheckFailures(const RoadGroundField &input) {
  const auto invalid = FitRoadGround(input, std::numeric_limits<double>::quiet_NaN());
  CHECK(!invalid && invalid.error() == RoadProfiles::HeightFitError::InvalidInput,
        "nonfinite original quality is rejected before either fit");
  CHECK(!FitRoadGround(input, -1), "negative prior changes cannot admit a field");
  CHECK(!FitRoadGround(input, 0) && !FitRoadGround(input, 1001),
        "selection preserves the positive bounded quality domain of the existing reference");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = FitRoadGround(input, kLimitM, {}, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadProfiles::HeightFitError::Canceled,
        "cancellation precedes affine preparation and smooth selection");
  const auto capped = FitRoadGround(input, kLimitM, {.ContainerBytes = 0});
  CHECK(!capped && capped.error() == RoadProfiles::HeightFitError::CapacityExceeded,
        "affine admission failure is explicit rather than bypassed through a different method");
  auto missing = input;
  missing.Priors[0].HeightM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!FitRoadGround(missing, kLimitM), "missing original terrain remains an explicit failure");
  auto malformed = input;
  malformed.Faces[0].Triangle[0].EastM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!FitRoadGround(malformed, kLimitM),
        "the cheap affine path cannot publish a malformed original face domain");
  malformed = input;
  malformed.Faces[0].Triangle[1] = malformed.Faces[0].Triangle[0];
  CHECK(
      !FitRoadGround(malformed, kLimitM),
      "a collapsed original domain cannot bypass smooth geometry checks through affine selection");
}
}

int main() {
  const auto flat = Input(false);
  const auto hill = Input(true);
  CHECK(flat && hill, "both full-width fields use the real native strip and field constructors");
  if (flat && hill) {
    CheckAffine(*flat);
    CheckMesh(*flat);
    CheckSmooth(*hill);
    CheckFailures(*flat);
  }
  return Report();
}
