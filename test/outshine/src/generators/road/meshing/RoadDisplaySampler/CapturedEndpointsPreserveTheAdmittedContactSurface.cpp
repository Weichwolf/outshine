#include "RoadDisplaySampler.h"
#include "RoadRibbonMesh.h"
#include "RoadCrossSection.h"
#include "Check.h"
#include "Json.h"
#include "TangentFrame.h"
#include "math/RenderFrame.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

Vec3 Vertex(const RoadMeshBuffers &mesh, size_t station, size_t across) {
  const size_t first = (station * kRibbonAcross + across) * 3;
  return {{mesh.PositionM[first], mesh.PositionM[first + 1], mesh.PositionM[first + 2]}};
}

void CheckApproach(const std::array<RoadStation, 2> &stations, double width) {
  const double dx = stations[1].EastM - stations[0].EastM;
  const double dy = stations[1].NorthM - stations[0].NorthM;
  const double length = std::hypot(dx, dy);
  CHECK(length > 0 && width > 0, "captured endpoint inputs are nondegenerate");
  if (length <= 0 || width <= 0) { return; }
  const double heading = std::atan2(dy, dx);
  const double slope = (stations[1].GradeM - stations[0].GradeM) / length;
  const Section section = RoadSection(width / 2, RoadProfile::Simple);
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({.EastM = stations[0].EastM, .NorthM = stations[0].NorthM, .HeadingRad = heading},
                 {{.LengthM = length}},
                 error) &&
            line.Rise({{.AlongM = 0, .Value = stations[0].GradeM, .RatePerM = slope},
                       {.AlongM = length, .Value = stations[1].GradeM, .RatePerM = slope}},
                      error),
        "a straight endpoint-control approach retains captured positions and heights");
  RoadSweep surface{.HalfWidthM = width / 2,
                    .Profile = RoadProfile::Simple,
                    .WearsLinear = {},
                    .Form = RibbonForm::Surface};
  const RoadPlane plane{.SlopeE = slope * std::cos(heading), .SlopeN = slope * std::sin(heading)};
  for (size_t end = 0; end < 2; ++end) {
    surface.EndPorts[end] =
        RoadPort{.Gate = RoadEndGate(stations, end != 0, section), .Plane = plane};
  }
  const auto contacts = RoadRibbonContacts(surface);
  const auto plan = SampleRoadDisplay(
      line, section, 0.01, {.StationsMost = 64, .PoseQueriesMost = 1024}, contacts);
  CHECK(plan && plan->StationsM.size() == 2,
        "actual coordinate precision fits a complete coarse approach");
  if (!plan) { return; }
  const auto ribbon = Sweep(line, section, 0, length, length, RibbonForm::Surface, plan->StationsM);
  CHECK(ribbon.Woven, "native Upper generation consumes the complete plan");
  if (!ribbon.Woven) { return; }
  RoadMeshBuffers mesh;
  AppendRoadRibbon(ribbon, surface, mesh);
  const std::array across{-section.HalfWidthM - section.ShoulderM,
                          -section.HalfWidthM,
                          section.HalfWidthM,
                          section.HalfWidthM + section.ShoulderM};
  double maximum = 0;
  for (size_t sample = 0; sample <= 20; ++sample) {
    const double t = static_cast<double>(sample) / 20;
    for (size_t band = 0; band + 1 < kRibbonAcross; ++band) {
      for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const double side = std::lerp(across[band], across[band + 1], u);
        const Vec3 expected{
            {std::lerp(stations[0].EastM, stations[1].EastM, t) - std::sin(heading) * side,
             std::lerp(stations[0].GradeM, stations[1].GradeM, t),
             RenderFrame::ZOfNorth(std::lerp(stations[0].NorthM, stations[1].NorthM, t) +
                                   std::cos(heading) * side)}};
        const Vec3 a = Vertex(mesh, 0, band);
        const Vec3 b = Vertex(mesh, 1, band);
        const Vec3 c = Vertex(mesh, 0, band + 1);
        const Vec3 d = Vertex(mesh, 1, band + 1);
        const Vec3 actual = t + u <= 1 ? a * (1 - t - u) + b * t + c * u
                                       : c * (1 - t) + b * (1 - u) + d * (t + u - 1);
        const Vec3 delta = actual - expected;
        maximum = std::max(maximum, std::hypot(delta[0], delta[1], delta[2]));
      }
    }
  }
  CHECK(maximum <= plan->PositionBoundM + 1e-9,
        "actual stitched source-frame triangles obey the position model plus explicit observation "
        "slack");
}
}

int main() {
  std::ifstream file("test/outshine/data/transport/wien-junction-sources.json");
  const std::string text(std::istreambuf_iterator<char>{file}, {});
  Json document;
  CHECK(document.Parse(text.data(), text.size()), "the retained actual OSM/DEM input is available");
  const auto root = document.Root();
  const auto anchor = root["reference_lat_lon"];
  const auto frame =
      TangentFrame::At({.LongitudeDeg = anchor[1].Num(), .LatitudeDeg = anchor[size_t{0}].Num()});
  const auto ways = root["ways"];
  CHECK(ways.Size() == 4, "all four captured endpoint controls are retained");
  for (size_t way = 0; way < ways.Size(); ++way) {
    const auto points = ways[way]["points_lat_lon"];
    const auto heights = ways[way]["source_ground_height_m"];
    CHECK(points.Size() >= 2 && heights.Size() == points.Size(),
          "captured coordinates have sourced heights");
    if (points.Size() < 2 || heights.Size() != points.Size()) { continue; }
    std::array<RoadStation, 2> stations;
    for (size_t end = 0; end < 2; ++end) {
      const size_t at = end == 0 ? 0 : points.Size() - 1;
      const auto position =
          frame.ToLocalPosition(LongitudeLatitudeHeight{.LongitudeDeg = points[at][1].Num(),
                                                        .LatitudeDeg = points[at][size_t{0}].Num(),
                                                        .HeightM = heights[at].Num()});
      stations[end] = {.EastM = position.EastM, .NorthM = position.NorthM, .GradeM = position.UpM};
    }
    CheckApproach(stations, ways[way]["width_m"].Num());
  }
  return Report();
}
