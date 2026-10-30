#include "Check.h"
#include "MvtGroundFieldScopeInternal.h"
#include "TransportNetworkSnapshot.h"
#include <array>
#include <memory>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

void CheckCoverage(std::vector<GeographicBounds> boxes,
                   std::array<LongitudeLatitude, 2> ends,
                   bool expected,
                   bool cancel = false) {
  std::vector<World::TransportNode> nodes{
      {.SourceNodeId = 1, .LatitudeDeg = ends[0].LatitudeDeg, .LongitudeDeg = ends[0].LongitudeDeg},
      {.SourceNodeId = 2,
       .LatitudeDeg = ends[1].LatitudeDeg,
       .LongitudeDeg = ends[1].LongitudeDeg}};
  auto network = std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{
          .Graph = World::TransportTopology(
              {.DatasetId = "controlled-coverage", .Revision = "1"}, std::move(nodes), {}),
          .Routes = {}},
      std::move(boxes),
      World::TransportLoadMetrics{});
  MvtTransportSnapshot snapshot;
  snapshot.Network = network;
  const MvtGroundFieldCandidateInputs inputs;
  const MvtTransportSourceFacts facts;
  const MvtGroundFieldLimits limits;
  std::stop_source cancellation;
  if (cancel) { (void)cancellation.request_stop(); }
  const auto stop = cancellation.get_token();
  GroundFieldScopes::State state{.Snapshot = snapshot,
                                 .Inputs = inputs,
                                 .Facts = facts,
                                 .Limits = limits,
                                 .Stop = stop,
                                 .Output = {},
                                 .NodeOwners = {},
                                 .EdgeOwners = {},
                                 .Ways = {},
                                 .Groups = {},
                                 .Seen = {},
                                 .KnownUnknown = {}};
  state.Output.Field.Source.Source = network;
  state.Output.Field.Source.Nodes = {0, 1};
  const auto result = GroundFieldScopes::Coverage(state);
  CHECK(result.has_value() == expected,
        "finite ground coverage uses the actual union and the shortest longitude segment");
  if (!expected) {
    CHECK(result.error() ==
              (cancel ? MvtGroundFieldError::Canceled : MvtGroundFieldError::CoverageGap),
          "exterior, open gaps, missing quadrants and cancellation retain their exact failures");
  }
}
}

int main() {
  const GeographicBounds left{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 0, .NorthDeg = 1};
  const GeographicBounds right{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1};
  constexpr LongitudeLatitude center{.LongitudeDeg = 0, .LatitudeDeg = 0};
  constexpr LongitudeLatitude east{.LongitudeDeg = .5, .LatitudeDeg = 0};
  CheckCoverage({left, right}, {center, east}, true);
  CheckCoverage({right, left}, {center, east}, true);
  CheckCoverage({left, right}, {{{.LongitudeDeg = -.5, .LatitudeDeg = 0}, east}}, true);
  CheckCoverage({left}, {center, {.LongitudeDeg = -.5, .LatitudeDeg = 0}}, false);
  CheckCoverage({left, right}, {{{.LongitudeDeg = -1, .LatitudeDeg = 0}, east}}, false);
  CheckCoverage({}, {center, east}, false);
  CheckCoverage({left, right}, {center, east}, false, true);
  CheckCoverage({{.WestDeg = -1, .SouthDeg = -1, .EastDeg = -.125, .NorthDeg = 1},
                 {.WestDeg = .125, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}},
                {{{.LongitudeDeg = -.5, .LatitudeDeg = 0}, east}},
                false);
  CheckCoverage({{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 0, .NorthDeg = 0},
                 {.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}},
                {center, {.LongitudeDeg = .5, .LatitudeDeg = .5}},
                false);
  std::vector<GeographicBounds> quarters;
  for (const double west : {-1., 0.}) {
    for (const double south : {-1., 0.}) {
      quarters.push_back(
          {.WestDeg = west, .SouthDeg = south, .EastDeg = west + 1, .NorthDeg = south + 1});
    }
  }
  CheckCoverage(quarters, {center, {.LongitudeDeg = .5, .LatitudeDeg = .5}}, true);
  const std::vector<GeographicBounds> wrapped{
      {.WestDeg = 179, .SouthDeg = -1, .EastDeg = 180, .NorthDeg = 1},
      {.WestDeg = -180, .SouthDeg = -1, .EastDeg = -179, .NorthDeg = 1}};
  for (const double seam : {-180., 180.}) {
    CheckCoverage(
        wrapped,
        {{{.LongitudeDeg = seam, .LatitudeDeg = 0}, {.LongitudeDeg = 179.5, .LatitudeDeg = 0}}},
        true);
  }
  CheckCoverage(
      wrapped,
      {{{.LongitudeDeg = 179.5, .LatitudeDeg = 0}, {.LongitudeDeg = -179.5, .LatitudeDeg = 0}}},
      true);
  return Report();
}
