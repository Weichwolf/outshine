#include "SurfaceFlightPlan.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

SurfaceHeightTopology Topology() {
  SurfaceHeightTopology topology;
  constexpr double kExistingStation = 2 + 1.0 / 4096;
  topology.Points = {{0, 0},
                     {2, 0},
                     {8, 0},
                     {10, 0},
                     {0, 1},
                     {2, 1},
                     {8, 1},
                     {10, 1},
                     {kExistingStation, 3},
                     {kExistingStation, 4},
                     {3, 4}};
  topology.Sources = {{.Faces = {{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}},
                       .PlanarFaces = {0, 1, 2, 3, 4, 5}},
                      {.Faces = {{8, 9, 10}}, .PlanarFaces = {6}}};
  topology.PortVertices = {{0, 1, 4, 5, 8}, {2, 3, 6, 7}};
  return topology;
}

std::vector<SurfaceHeightRecipe> Sources() {
  return {{.WidthM = 1,
           .MaximumGrade = 1,
           .Steps = true,
           .Axis = {{0, 0}, {10, 0}},
           .Prior = {},
           .Sections = {},
           .Domain = {}},
          {.WidthM = 1,
           .MaximumGrade = 1,
           .Steps = false,
           .Axis = {{0, 3}, {10, 3}},
           .Prior = {},
           .Sections = {},
           .Domain = {}}};
}

void QualifiedLandingsRetainUnoccupiedCutIntervals() {
  const auto topology = Topology();
  const auto sources = Sources();
  const std::array endpoints{
      SurfaceFlightEndpoints{.Source = 0, .Start = 0, .End = 1, .Direction = -1}};
  const auto plan = BuildSurfaceFlightPlan(topology, sources, endpoints);
  CHECK(plan && plan->Flights.size() == 1 && plan->Axes.size() == 1,
        "one explicit source and two admitted landings produce one owned flight");
  if (!plan) { return; }
  const auto &flight = plan->Flights[0];
  constexpr double kExpectedStart = 2 + 5.0 / 8192;
  constexpr double kExpectedEnd = 8 - 1.0 / 2048;
  CHECK(flight.CutStart == kExpectedStart && flight.CutEnd == kExpectedEnd,
        "the largest unoccupied interval includes stations belonging to other surface sources");
  CHECK(flight.Direction == -1 && flight.Start == 0 && flight.End == 1,
        "axis endpoints remain distinct from the declared ascending or descending direction");
  CHECK(flight.Stations.size() == topology.Points.size() && flight.Stations[2] == 8,
        "the complete prepared height topology receives indexed station values");
  CHECK((plan->Axes[0].Cuts == std::vector<double>{flight.CutStart, flight.CutEnd}),
        "the geometric cutter consumes exactly the planner's two cuts");
  CHECK(plan->Work.PointProjections == topology.Points.size(),
        "one flight projects each available point once");
}

void UnsupportedFlightsNeverReturnPartialPlans() {
  auto topology = Topology();
  auto sources = Sources();
  std::array endpoints{SurfaceFlightEndpoints{.Source = 0, .Start = 0, .End = 1, .Direction = 1}};
  SurfaceFlightBudget budget;
  budget.VertexVisits = 1;
  const auto capped = BuildSurfaceFlightPlan(topology, sources, endpoints, budget);
  CHECK(!capped && capped.error() == SurfaceFlightError::CapacityExceeded,
        "bounded work stops before returning a partly evaluated flight");
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped =
      BuildSurfaceFlightPlan(topology, sources, endpoints, {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == SurfaceFlightError::Canceled,
        "cancellation prevents flight preparation");
  topology.PortVertices[0] = {8};
  const auto missing = BuildSurfaceFlightPlan(topology, sources, endpoints);
  CHECK(!missing && missing.error() == SurfaceFlightError::MissingLanding,
        "a geometrically nearby vertex from another source cannot own the stair landing");
  topology = Topology();
  sources[0].Axis = {{0, 0}, {5, 1}, {10, 0}};
  const auto curved = BuildSurfaceFlightPlan(topology, sources, endpoints);
  CHECK(!curved && curved.error() == SurfaceFlightError::CurvedAxis,
        "a curved route requires separately planned straight flights");
  sources = Sources();
  endpoints[0].Start = 1;
  endpoints[0].End = 0;
  const auto backwards = BuildSurfaceFlightPlan(topology, sources, endpoints);
  CHECK(!backwards && backwards.error() == SurfaceFlightError::NoSlopedRun,
        "overlapping or reversed landing extents do not create a negative sloped run");
}
}

int main() {
  QualifiedLandingsRetainUnoccupiedCutIntervals();
  UnsupportedFlightsNeverReturnPartialPlans();
  return Report();
}
