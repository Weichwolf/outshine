#include "Check.h"
#include "SurfaceHeightProgram.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kCrossfall = .2;
constexpr double kHeightToleranceM = 1e-9;

struct Prepared {
  SurfaceHeightTopology Topology;
  std::vector<SurfaceHeightRecipe> Sources;
  std::vector<SurfaceHeightJunction> Ports;
  std::vector<SurfaceHeightFlight> Flights;
  std::vector<SurfaceHeightContact> Contacts;
  std::vector<SurfaceHeightOrder> Orders;

  [[nodiscard]] SurfaceHeightRequest Request(double adjustment) const {
    return {.Topology = Topology,
            .Sources = Sources,
            .Ports = Ports,
            .Flights = Flights,
            .Contacts = Contacts,
            .Orders = Orders,
            .MaximumAdjustmentM = adjustment,
            .MaximumCrossfall = kCrossfall};
  }
};

Prepared Road() {
  Prepared input;
  input.Topology.Points = {{0, 0}, {10, 0}, {10, 2}, {0, 2}, {5, 0}, {5, 2}};
  input.Topology.Sources = {
      {.Faces = {{0, 4, 5}, {0, 5, 3}, {4, 1, 2}, {4, 2, 5}}, .PlanarFaces = {0, 1, 2, 3}}};
  input.Sources = {{.WidthM = 2,
                    .MaximumGrade = .5,
                    .Steps = false,
                    .Axis = {{0, 1}, {10, 1}},
                    .Prior = {{0, 1, 0}, {5, 1, 1}, {10, 1, 0}},
                    .Sections = {},
                    .Domain = {.Rings = {{{0, 0}, {10, 0}, {10, 2}, {0, 2}}}}}};
  return input;
}

void CompleteProfilesConstrainTheirInterior() {
  auto input = Road();
  const auto free = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(free.has_value(), "bounded directional surface assembly accepts an unconstrained crown");
  if (!free) { return; }
  CHECK(RoadProfiles::FitHeights(free->Program, {}, {}).has_value(),
        "the unchanged prior crown is feasible without a fixed outer profile");
  input.Contacts = {{.Source = 0, .Points = {{0, 1, 0}, {10, 1, 0}}}};
  const auto fixed = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(fixed.has_value(), "whole-profile contact assembly includes triangle intersections");
  if (!fixed) { return; }
  const auto impossible = RoadProfiles::FitHeights(fixed->Program, {}, {});
  CHECK(!impossible && impossible.error() == RoadProfiles::HeightFitError::Infeasible,
        "a conflicting profile interior is rejected even without a supplied midpoint knot");
  input.Sources[0].Prior[1][2] = 0;
  const auto flat = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(flat.has_value(), "a compatible full fixed profile remains admissible");
  if (!flat) { return; }
  const auto fit = RoadProfiles::FitHeights(flat->Program, {}, {});
  CHECK(fit.has_value(), "the compatible physical field solves");
  if (!fit) { return; }
  const auto expanded = flat->Projection.Multiply(fit->Values);
  for (size_t at = 0; at < flat->PointCount; ++at) {
    CHECK_NEAR(expanded[at] + flat->DatumM,
               0,
               kHeightToleranceM,
               "m",
               "native expansion preserves the full profile");
  }
}

Prepared TwoFlights() {
  constexpr double kStairGrade = .8;
  Prepared input;
  input.Topology.Points = {{0, 0}, {2, 0}, {2, 1}, {0, 2}, {-1, 2}};
  input.Topology.Sources = {{.Faces = {{0, 1, 2}}, .PlanarFaces = {0}},
                            {.Faces = {{0, 3, 4}}, .PlanarFaces = {1}}};
  input.Sources = {{.WidthM = 1,
                    .MaximumGrade = kStairGrade,
                    .Steps = true,
                    .Axis = {{0, 0}, {2, 0}},
                    .Prior = {{0, 0, 0}, {2, 0, 1}},
                    .Sections = {std::array{PlanarPoint{2, 0}, PlanarPoint{2, 1}}},
                    .Domain = {.Rings = {{{0, 0}, {2, 0}, {2, 1}}}}},
                   {.WidthM = 1,
                    .MaximumGrade = kStairGrade,
                    .Steps = true,
                    .Axis = {{0, 0}, {0, 2}},
                    .Prior = {{0, 0, 0}, {0, 2, 1}},
                    .Sections = {std::array{PlanarPoint{0, 2}, PlanarPoint{-1, 2}}},
                    .Domain = {.Rings = {{{0, 0}, {0, 2}, {-1, 2}}}}}};
  input.Ports = {{.Origin = {0, 0}, .Vertices = {0}},
                 {.Origin = {2, 0}, .Vertices = {1, 2}},
                 {.Origin = {0, 2}, .Vertices = {3, 4}}};
  input.Flights = {{.Source = 0,
                    .Start = 0,
                    .End = 1,
                    .Direction = 1,
                    .CutStart = 0,
                    .CutEnd = 2,
                    .Stations = {0, 2, 2, 0, 0}},
                   {.Source = 1,
                    .Start = 0,
                    .End = 2,
                    .Direction = 1,
                    .CutStart = 0,
                    .CutEnd = 2,
                    .Stations = {0, 0, 0, 2, 2}}};
  input.Contacts = {{.Source = 0, .Points = {{0, 0, 0}, {2, 0, 1}}},
                    {.Source = 1, .Points = {{0, 0, 0}, {0, 2, 1}}}};
  return input;
}

void SharedStartHasOneEffectiveHeightOwner() {
  auto input = TwoFlights();
  const auto assembled = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(assembled.has_value(), "zero endpoint weights do not invent conflicting flight ownership");
  if (!assembled) { return; }
  CHECK(assembled->Projection.Columns == 10 && assembled->Projection.Rows == 15,
        "all five derived point heights are eliminated before numerical fitting");
  const auto fit = RoadProfiles::FitHeights(assembled->Program, {}, {});
  CHECK(fit.has_value(), "two independent flights can share one explicit lower junction");
  if (!fit) { return; }
  const auto expanded = assembled->Projection.Multiply(fit->Values);
  for (size_t at = 0; at < assembled->PointCount; ++at) {
    CHECK_NEAR(expanded[at] + assembled->DatumM,
               at == 0 ? 0 : 1,
               kHeightToleranceM,
               "m",
               "the two high junctions and common low junction retain their fixed heights");
  }
  input.Flights[1].Stations[0] = 1;
  const auto conflict = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(!conflict && conflict.error() == SurfaceHeightProgramError::ConflictingOwners,
        "different effective affine owners remain an explicit error");
}

void OverlappingLevelsRequireDeclaredPhysicalOrder() {
  auto input = Road();
  for (auto &point : input.Sources[0].Prior) { point[2] = 0; }
  const auto lowerPoints = input.Topology.Points;
  input.Topology.Points.insert(input.Topology.Points.end(), lowerPoints.begin(), lowerPoints.end());
  auto upperFaces = input.Topology.Sources[0];
  for (auto &face : upperFaces.Faces) {
    for (auto &vertex : face) { vertex += 6; }
  }
  input.Topology.Sources.push_back(upperFaces);
  auto upper = input.Sources[0];
  for (auto &point : upper.Prior) { point[2] = 2; }
  input.Sources.push_back(upper);
  const auto unordered = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(!unordered && unordered.error() == SurfaceHeightProgramError::InvalidInput,
        "coincident geometric subfaces do not silently choose a source height order");
  input.Orders = {{.Lower = 0, .Upper = 1}};
  const auto ordered = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(ordered && ordered->Work.OverlapRows == 12,
        "every corner of each shared geometric child receives the explicit level order");
  if (!ordered) { return; }
  CHECK(RoadProfiles::FitHeights(ordered->Program, {}, {}).has_value(),
        "independent levels retain their own prior heights and shared planar identities");
  input.Orders = {{.Lower = 1, .Upper = 0}};
  const auto reversed = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(reversed.has_value(), "the order is a physical condition rather than a geometry merge");
  if (!reversed) { return; }
  const auto impossible = RoadProfiles::FitHeights(reversed->Program, {}, {});
  CHECK(!impossible && impossible.error() == RoadProfiles::HeightFitError::Infeasible,
        "an order conflicting with fixed source priors cannot pass the original constraints");
}

void BoundedInputsNeverPublishPartialPrograms() {
  auto input = Road();
  SurfaceHeightProgramBudget budget;
  budget.Conditions = 1;
  const auto capped = BuildSurfaceHeightProgram(input.Request(0), budget);
  CHECK(!capped && capped.error() == SurfaceHeightProgramError::CapacityExceeded,
        "assembly stops before publishing a truncated constraint set");
  budget = {};
  budget.PriorTests = 0;
  const auto projectionLimit = BuildSurfaceHeightProgram(input.Request(0), budget);
  CHECK(!projectionLimit && projectionLimit.error() == SurfaceHeightProgramError::CapacityExceeded,
        "bounded closest-segment work cannot silently use an unfinished prior field");
  budget = {};
  budget.ProjectionProducts = 0;
  const auto multiplicationLimit = BuildSurfaceHeightProgram(input.Request(0), budget);
  CHECK(!multiplicationLimit &&
            multiplicationLimit.error() == SurfaceHeightProgramError::CapacityExceeded,
        "sparse projection work is bounded before allocating the product matrix");
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped = BuildSurfaceHeightProgram(input.Request(0), {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == SurfaceHeightProgramError::Canceled,
        "pre-canceled input never starts geometric or solver work");
  input.Topology.Sources[0].Faces[0][0] = static_cast<uint32_t>(input.Topology.Points.size());
  const auto invalid = BuildSurfaceHeightProgram(input.Request(0));
  CHECK(!invalid && invalid.error() == SurfaceHeightProgramError::InvalidInput,
        "untrusted vertex references are rejected before geometric access");
}
}

int main() {
  CompleteProfilesConstrainTheirInterior();
  SharedStartHasOneEffectiveHeightOwner();
  OverlappingLevelsRequireDeclaredPhysicalOrder();
  BoundedInputsNeverPublishPartialPrograms();
  return Report();
}
