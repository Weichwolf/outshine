#include "SurfaceFlightPreparation.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
}

int main() {
  const PlanarArrangement original{
      .Mesh = {.Points = {{0, 0}, {2, 0}, {8, 0}, {10, 0}, {0, 2}, {2, 2}, {8, 2}, {10, 2}},
               .Faces = {{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}},
               .PointRemap = {},
               .FaceParents = {},
               .Work = {}},
      .FaceDomains = {3, 3, 3, 3, 3, 3},
      .Work = {}};
  const std::array ports{
      SurfaceHeightPort{.Region = {.Rings = {{{-1, -1}, {2, -1}, {2, 3}, {-1, 3}}}},
                        .Domains = {0}},
      SurfaceHeightPort{.Region = {.Rings = {{{8, -1}, {11, -1}, {11, 3}, {8, 3}}}},
                        .Domains = {0}}};
  const std::array sources{SurfaceHeightRecipe{.WidthM = 2,
                                               .MaximumGrade = 1,
                                               .Steps = true,
                                               .Axis = {{0, 1}, {10, 1}},
                                               .Prior = {},
                                               .Sections = {},
                                               .Domain = {}},
                           SurfaceHeightRecipe{.WidthM = 2,
                                               .MaximumGrade = 1,
                                               .Steps = false,
                                               .Axis = {{0, 1}, {10, 1}},
                                               .Prior = {},
                                               .Sections = {},
                                               .Domain = {}}};
  const std::array flights{
      SurfaceFlightEndpoints{.Source = 0, .Start = 0, .End = 1, .Direction = 1}};
  const auto prepared = PrepareSurfaceFlights(original, 2, ports, 0, sources, flights);
  CHECK(prepared.has_value(),
        "native qualified ports feed flight selection, indexed cuts and final source heights");
  if (!prepared) { return Report(); }
  const auto &mesh = prepared->Arrangement.Mesh;
  const auto &height = prepared->Heights;
  const auto &plan = prepared->Plan;
  CHECK(mesh.Points.size() > original.Mesh.Points.size() && plan.Flights.size() == 1,
        "the selected landing boundaries refine geometry before its height program is built");
  CHECK(std::equal(original.Mesh.Points.begin(), original.Mesh.Points.end(), mesh.Points.begin()),
        "all preexisting native positions remain exact through preparation");
  CHECK(height.Sources[0].PlanarFaces == height.Sources[1].PlanarFaces,
        "overlapping source levels retain the same distinct geometric children");
  CHECK(plan.Flights[0].Stations.size() == height.Points.size() &&
            prepared->StationBindings == height.Points.size(),
        "every final height variable receives the indexed native station value");
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    CHECK(prepared->Arrangement.FaceDomains[at] == original.FaceDomains[mesh.FaceParents[at]],
          "station subdivision preserves the source mask of its original face");
    const auto a = height.Sources[0].Faces[at];
    const auto b = height.Sources[1].Faces[at];
    for (size_t corner = 0; corner < a.size(); ++corner) {
      CHECK(a[corner] != b[corner] &&
                height.PlanarVertices[a[corner]] == height.PlanarVertices[b[corner]],
            "a cut cannot join unrelated height levels at the same geometric point");
    }
    for (const auto cut : plan.Axes[0].Cuts) {
      std::array<double, 3> stations{};
      for (size_t corner = 0; corner < a.size(); ++corner) {
        stations[corner] = plan.Flights[0].Stations[a[corner]];
      }
      CHECK(std::ranges::min(stations) >= cut || std::ranges::max(stations) <= cut,
            "no final source facet crosses a declared flat-to-sloped transition");
    }
  }
  for (const auto &members : height.PortVertices) {
    CHECK(!members.empty(), "both qualified landing lists survive native subdivision");
    for (const auto member : members) {
      CHECK(height.PrimaryDomains[member] == 0,
            "unqualified upper source cannot enter a landing height list");
    }
  }
  auto overlapping = ports;
  overlapping[1].Region = overlapping[0].Region;
  const auto invalid = PrepareSurfaceFlights(original, 2, overlapping, 0, sources, flights);
  CHECK(!invalid && std::holds_alternative<SurfaceFlightError>(invalid.error()) &&
            std::get<SurfaceFlightError>(invalid.error()) == SurfaceFlightError::NoSlopedRun,
        "a failed preparation preserves its precise stage/reason without publishing partial "
        "geometry");
  return Report();
}
