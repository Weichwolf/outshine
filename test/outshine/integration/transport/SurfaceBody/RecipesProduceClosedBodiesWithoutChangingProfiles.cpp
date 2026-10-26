#include "Check.h"
#include "SurfaceBody.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
constexpr double kToleranceM = 1e-10;
constexpr double kFlightHeightM = 1.2;
constexpr double kNonplanarErrorM = .01;

struct Fixture {
  SurfaceHeightTopology Topology;
  std::vector<SurfaceHeightRecipe> Sources;
  std::vector<SurfaceHeightJunction> Ports;
  std::vector<SurfaceHeightFlight> Flights;
  std::vector<double> Heights;

  [[nodiscard]] SurfaceHeightRequest Request() const {
    return {.Topology = Topology,
            .Sources = Sources,
            .Ports = Ports,
            .Flights = Flights,
            .Contacts = {},
            .Orders = {},
            .MaximumAdjustmentM = 0,
            .MaximumCrossfall = 0};
  }
};

Fixture Flight() {
  Fixture input;
  for (uint32_t x = 0; x <= 4; ++x) {
    input.Topology.Points.push_back({static_cast<double>(x), 0});
    input.Topology.Points.push_back({static_cast<double>(x), 1});
    const double height = kFlightHeightM * std::clamp((static_cast<double>(x) - 1) / 2, 0.0, 1.0);
    input.Heights.insert(input.Heights.end(), 2, height);
  }
  input.Topology.Sources.resize(1);
  for (uint32_t x = 0; x < 4; ++x) {
    const uint32_t first = x * 2;
    input.Topology.Sources[0].Faces.push_back({first, first + 2, first + 3});
    input.Topology.Sources[0].Faces.push_back({first, first + 3, first + 1});
  }
  input.Sources.resize(1);
  input.Sources[0].Steps = true;
  input.Sources[0].Axis = {{0, .5}, {4, .5}};
  input.Ports = {{.Origin = {0, .5}, .Vertices = {0, 1}}, {.Origin = {4, .5}, .Vertices = {8, 9}}};
  input.Flights = {{.Source = 0,
                    .Start = 0,
                    .End = 1,
                    .Direction = 1,
                    .CutStart = 1,
                    .CutEnd = 3,
                    .Stations = {}}};
  return input;
}

bool HasError(const std::expected<SurfaceBody, SurfaceBodyError> &result,
              SurfaceBodyPlanError expected) {
  return !result && std::holds_alternative<SurfaceBodyPlanError>(result.error()) &&
         std::get<SurfaceBodyPlanError>(result.error()) == expected;
}

void OneFlightKeepsItsContinuousFieldAndProducesFlatTreads() {
  const auto input = Flight();
  const auto body = BuildSurfaceBody(input.Request(), input.Heights);
  CHECK(body.has_value(), "one request builds recipes, regions, height cuts and a closed body");
  if (!body) { return; }
  CHECK(body->Stairs.size() == 1 && body->Stairs[0].Risers == 7,
        "rise bound selects seven steps for the original 1.2m interval");
  CHECK_NEAR(body->Levels.back(), kFlightHeightM, kToleranceM, "m", "upper landing remains exact");
  CHECK(body->Geometry.VolumeM3 > 4 * SurfaceBodyBudget::kDefaultThicknessM,
        "closed body contains the slab and the material under its treads");
  CHECK(body->Geometry.SurfaceFaces == body->ReferenceFaces.size(),
        "every final reference face has one top and bottom face");
  CHECK(body->Work.Envelope.OutputFaces < input.Topology.Sources[0].Faces.size(),
        "coplanar envelopes are simplified before producing tread geometry");
  CHECK(body->Work.MaximumPlaneErrorM <= SurfaceBodyBudget::kMaximumPlaneErrorM,
        "all remeshed faces retain the reference height plane");
  for (size_t at = 0; at < body->ReferenceFaces.size(); ++at) {
    CHECK(body->FaceSources[at] == 1, "source identity survives both remesh passes");
    const int32_t level = body->FaceLevels[at];
    CHECK(level >= 0 && static_cast<size_t>(level) < body->Levels.size(),
          "each stair face belongs to one declared height level");
    const auto top = body->Geometry.Faces[at];
    for (const auto vertex : top) {
      CHECK_NEAR(body->Geometry.Points[vertex][2],
                 body->Levels[static_cast<size_t>(level)],
                 kToleranceM,
                 "m",
                 "each displayed tread is exactly flat");
    }
  }
}

void TwoIndependentSheetsKeepTheirOwnLevels() {
  auto input = Flight();
  const auto second = Flight();
  const auto offset = static_cast<uint32_t>(input.Topology.Points.size());
  input.Topology.Points.insert(
      input.Topology.Points.end(), second.Topology.Points.begin(), second.Topology.Points.end());
  input.Topology.Sources.push_back(second.Topology.Sources[0]);
  for (auto &face : input.Topology.Sources[1].Faces) {
    for (auto &vertex : face) { vertex += offset; }
  }
  input.Sources.push_back(second.Sources[0]);
  input.Ports.insert(input.Ports.end(), second.Ports.begin(), second.Ports.end());
  input.Flights.push_back(second.Flights[0]);
  input.Flights[1].Source = 1;
  input.Flights[1].Start = 2;
  input.Flights[1].End = 3;
  for (const double height : second.Heights) { input.Heights.push_back(height + 2); }
  const auto body = BuildSurfaceBody(input.Request(), input.Heights);
  CHECK(body.has_value(), "coincident XY sheets close independently without height welding");
  if (!body) { return; }
  CHECK(body->Stairs.size() == 2, "both independently qualified flights have a recipe");
  CHECK_NEAR(body->Stairs[1].LowM, 2, kToleranceM, "m", "upper sheet keeps its own lower landing");
  for (size_t at = 0; at < body->FaceSources.size(); ++at) {
    const uint32_t source = body->FaceSources[at] == 1 ? 0 : 1;
    const auto recipe = body->Stairs[source];
    const auto level = static_cast<uint32_t>(body->FaceLevels[at]);
    CHECK(body->FaceSources[at] == (uint64_t{1} << source) && level >= recipe.FirstLevel &&
              level <= recipe.FirstLevel + recipe.Risers,
          "each face selects only its own source height range");
  }
}

void OrdinarySurfacesNeedNoProviderOrStairRecipe() {
  auto input = Flight();
  input.Flights.clear();
  input.Sources[0].Steps = false;
  const auto body = BuildSurfaceBody(input.Request(), input.Heights);
  CHECK(body.has_value(), "ordinary continuous surfaces close without inventing a stair recipe");
  if (!body) { return; }
  CHECK(body->ReferencePoints.size() == input.Heights.size() && body->Stairs.empty(),
        "ordinary source field retains all of its used samples");
  CHECK(std::ranges::all_of(body->FaceLevels, [](int32_t level) { return level == -1; }),
        "ordinary top faces retain their continuous height field");
  CHECK_NEAR(body->Geometry.VolumeM3,
             4 * SurfaceBodyBudget::kDefaultThicknessM,
             kToleranceM,
             "m3",
             "constant vertical thickness preserves the analytic slab volume");
}

void QualifiedSourceAndOperatorBudgetsRemainExplicit() {
  auto input = Flight();
  const auto source = input.Sources.front();
  const auto mesh = input.Topology.Sources.front();
  constexpr uint32_t lastSource = 63;
  input.Sources.assign(lastSource + 1, {});
  input.Topology.Sources.assign(lastSource + 1, {});
  input.Sources[lastSource] = source;
  input.Topology.Sources[lastSource] = mesh;
  input.Flights.front().Source = lastSource;
  const auto body = BuildSurfaceBody(input.Request(), input.Heights);
  CHECK(body.has_value(), "the highest qualified source bit remains usable");
  if (body) {
    CHECK(std::ranges::all_of(body->FaceSources,
                              [](uint64_t mask) { return mask == (uint64_t{1} << lastSource); }),
          "source masks retain the unsigned high bit through both group passes");
  }
  SurfaceBodyBudget budget;
  budget.Regions.Regions = 0;
  const auto limited = BuildSurfaceBody(input.Request(), input.Heights, budget);
  CHECK(!limited && std::holds_alternative<PlanarRegionError>(limited.error()) &&
            std::get<PlanarRegionError>(limited.error()) == PlanarRegionError::CapacityExceeded,
        "the body must honor an explicitly bounded remeshing budget");
}

void InvalidFieldsAndBoundedWorkNeverPublishPartialBodies() {
  auto input = Flight();
  SurfaceBodyBudget budget;
  budget.Visits = 0;
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights, budget),
                 SurfaceBodyPlanError::CapacityExceeded),
        "work limit stops the body atomically");
  std::stop_source stop;
  stop.request_stop();
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights, {}, stop.get_token()),
                 SurfaceBodyPlanError::Canceled),
        "cancellation never publishes partial geometry");
  input.Heights[4] += kNonplanarErrorM;
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights),
                 SurfaceBodyPlanError::NonplanarFlight),
        "a nonplanar flight is never flattened");
  input = Flight();
  input.Topology.Sources[0].Faces = {{0, 8, 9}, {0, 9, 1}};
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights),
                 SurfaceBodyPlanError::CrossingPlane),
        "uncut landing planes require preparation");
  input = Flight();
  input.Flights.clear();
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights),
                 SurfaceBodyPlanError::ConflictingOwners),
        "a stair source needs a qualified flight");
  input = Flight();
  input.Heights[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(HasError(BuildSurfaceBody(input.Request(), input.Heights),
                 SurfaceBodyPlanError::InvalidInput),
        "nonfinite heights cannot reach geometry");
}
}

int main() {
  OneFlightKeepsItsContinuousFieldAndProducesFlatTreads();
  TwoIndependentSheetsKeepTheirOwnLevels();
  OrdinarySurfacesNeedNoProviderOrStairRecipe();
  QualifiedSourceAndOperatorBudgetsRemainExplicit();
  InvalidFieldsAndBoundedWorkNeverPublishPartialBodies();
  return Report();
}
