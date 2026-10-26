#include "Check.h"
#include "SurfaceHeightProgram.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

struct Control {
  double Angle = 0;
  int Direction = 1;
};

void ConstantLanding(Control control) {
  constexpr double kThinHeightM = 1e-9;
  constexpr double kHeightToleranceM = 1e-9;
  const std::array<PlanarPoint, 9> local{
      {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {3, 0}, {3, 1}, {4, 0}, {4, 1}, {.3, kThinHeightM}}};
  const auto move = [angle = control.Angle](PlanarPoint point) {
    constexpr PlanarPoint kTranslationM{80, -300};
    return PlanarPoint{kTranslationM[0] + std::cos(angle) * point[0] - std::sin(angle) * point[1],
                       kTranslationM[1] + std::sin(angle) * point[0] + std::cos(angle) * point[1]};
  };
  SurfaceHeightTopology topology;
  for (const auto point : local) { topology.Points.push_back(move(point)); }
  topology.Sources = {
      {.Faces =
           {{0, 1, 8}, {1, 2, 8}, {2, 3, 8}, {3, 0, 8}, {1, 4, 5}, {1, 5, 2}, {4, 6, 7}, {4, 7, 5}},
       .PlanarFaces = {0, 1, 2, 3, 4, 5, 6, 7}}};
  const double start = control.Direction == 1 ? 0 : 1;
  const double end = 1 - start;
  const auto xyz = [](PlanarPoint point, double height) {
    return SurfaceHeightPoint{point[0], point[1], height};
  };
  const auto first = move({0, .5});
  const auto last = move({4, .5});
  const std::array<SurfaceHeightRecipe, 1> sources{{{.WidthM = 1,
                                                     .MaximumGrade = 1,
                                                     .Steps = true,
                                                     .Axis = {first, last},
                                                     .Prior = {xyz(first, start), xyz(last, end)},
                                                     .Sections = {},
                                                     .Domain = {.Rings = {{topology.Points[0],
                                                                           topology.Points[6],
                                                                           topology.Points[7],
                                                                           topology.Points[3]}}}}}};
  const std::array<SurfaceHeightJunction, 2> ports{
      {{.Origin = first, .Vertices = {0, 1, 2, 3, 8}}, {.Origin = last, .Vertices = {4, 5, 6, 7}}}};
  const std::array<SurfaceHeightFlight, 1> flights{{{.Source = 0,
                                                     .Start = 0,
                                                     .End = 1,
                                                     .Direction = control.Direction,
                                                     .CutStart = 1,
                                                     .CutEnd = 3,
                                                     .Stations = {0, 1, 1, 0, 3, 3, 4, 4, .3}}}};
  const std::array<SurfaceHeightContact, 2> contacts{
      {{.Source = 0, .Points = {xyz(topology.Points[0], start), xyz(topology.Points[3], start)}},
       {.Source = 0, .Points = {xyz(topology.Points[6], end), xyz(topology.Points[7], end)}}}};
  const auto program = BuildSurfaceHeightProgram({.Topology = topology,
                                                  .Sources = sources,
                                                  .Ports = ports,
                                                  .Flights = flights,
                                                  .Contacts = contacts,
                                                  .Orders = {},
                                                  .MaximumAdjustmentM = 1,
                                                  .MaximumCrossfall = .2});
  CHECK(program.has_value(), "a valid connected flight can contain a thin landing triangle");
  if (!program) { return; }
  const auto fit = RoadProfiles::FitHeights(program->Program, {}, {});
  CHECK(fit.has_value(), "constant landing heights cannot acquire a false monotonic gradient");
  if (!fit) { return; }
  CHECK(fit->MaximumViolation <= kHeightToleranceM,
        "the original-matrix verification bound remains unchanged");
  const auto heights = program->Projection.Multiply(fit->Values);
  for (size_t at = 0; at < local.size(); ++at) {
    const double fraction = std::clamp((local[at][0] - 1) / 2, 0., 1.);
    CHECK_NEAR(heights[at] + program->DatumM,
               start + fraction * (end - start),
               kHeightToleranceM,
               "m",
               "rotation and a thin triangle preserve the analytic flat-run-flat height field");
  }
}
}

int main() {
  for (const double angle : {0., .37}) {
    for (const int direction : {-1, 1}) {
      ConstantLanding({.Angle = angle, .Direction = direction});
    }
  }
  return outshine::Test::Report();
}
