#include "RoadDisplaySampler.h"
#include "RoadRibbonMesh.h"
#include "RoadCrossSection.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace {
void CheckBindings(const outshine::Ribbon &ribbon,
                   const outshine::RoadSweep &surface,
                   const outshine::Generators::RoadDisplayContacts &contacts) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RoadMeshBuffers mesh;
  AppendRoadRibbon(ribbon, surface, mesh);
  for (size_t end = 0; end < 2; ++end) {
    const auto &ring = contacts.EndRings[end];
    CHECK(ring.has_value(), "both complete source contact rings exist");
    if (!ring) { continue; }
    for (size_t across = 0; across < kRibbonAcross; ++across) {
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK(mesh.PositionM[(end * kRibbonAcross + across) * 3 + axis] == (*ring)[across][axis],
              "the production stitcher uses exactly the rings admitted by sampling");
      }
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double east = 8003.123456;
  constexpr double north = -7043.654321;
  constexpr double height = 503.234567;
  constexpr double length = 20;
  constexpr double slope = 0.07;
  constexpr double bank = 0.04;
  const Section section = RoadSection(3, RoadProfile::Simple);
  const std::array stations{
      RoadStation{.EastM = east, .NorthM = north, .GradeM = height},
      RoadStation{.EastM = east + length, .NorthM = north, .GradeM = height + slope * length}};
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({.EastM = east, .NorthM = north}, {{.LengthM = length}}, error) &&
            line.Rise({{.AlongM = 0, .Value = height, .RatePerM = slope},
                       {.AlongM = length, .Value = height + slope * length, .RatePerM = slope}},
                      error) &&
            line.Bank({{.AlongM = 0, .Value = bank}, {.AlongM = length, .Value = bank}}, error),
        "independent planar source with nontrivial origin and bank");
  RoadSweep surface{.HalfWidthM = 3,
                    .Profile = RoadProfile::Simple,
                    .WearsLinear = {},
                    .Form = RibbonForm::Surface};
  const RoadPlane plane{.SlopeE = slope, .SlopeN = -std::tan(bank)};
  for (size_t end = 0; end < 2; ++end) {
    RoadPort port{.Gate = RoadEndGate(stations, end != 0, section), .Plane = plane};
    if (end != 0) { port.Gate.GradeM += 0.2; }
    surface.EndPorts[end] = port;
  }
  const auto contacts = RoadRibbonContacts(surface);
  constexpr RoadDisplayLimits limits{.StationsMost = 1024, .PoseQueriesMost = 10000};
  const auto plan = SampleRoadDisplay(line, section, 0.25, limits, contacts);
  CHECK(plan && plan->StationsM.size() == 2,
        "ported affine Upper needs no unnecessary inner rings");
  if (plan) {
    CHECK(plan->ContactDisplacementM > 0.19 && plan->ContactDisplacementM <= plan->PositionBoundM &&
              plan->PositionBoundM <= 0.25,
          "contact displacement consumes the same requested position budget");
    const auto ribbon =
        Sweep(line, section, 0, length, length, RibbonForm::Surface, plan->StationsM);
    CHECK(ribbon.Woven, "the plan generates directly through the native ribbon");
    if (ribbon.Woven) { CheckBindings(ribbon, surface, contacts); }
  }
  auto moved = contacts;
  auto &movedRing = moved.EndRings[1];
  CHECK(movedRing.has_value(), "the moved end contact exists");
  if (movedRing) {
    for (auto &point : *movedRing) { point[1] += 1; }
  }
  CHECK(!SampleRoadDisplay(line, section, 0.25, limits, moved),
        "endpoint movement exceeding the budget cannot be hidden by finer subdivision");
  auto reversed = contacts;
  auto &reversedRing = reversed.EndRings[0];
  CHECK(reversedRing.has_value(), "the reversed end contact exists");
  if (reversedRing) { std::ranges::reverse(*reversedRing); }
  CHECK(!SampleRoadDisplay(line, section, 100, limits, reversed),
        "even a large metric budget cannot authorize a reversed endpoint ring");
  auto invalid = contacts;
  auto &invalidRing = invalid.EndRings[0];
  CHECK(invalidRing.has_value(), "the invalid end contact exists");
  if (invalidRing) { (*invalidRing)[0][0] = std::numeric_limits<float>::quiet_NaN(); }
  CHECK(!SampleRoadDisplay(line, section, 1, limits, invalid), "nonfinite contacts are rejected");
  CHECK(line.Lay({.EastM = 100000000}, {{.LengthM = length}}, error), "large-origin source");
  CHECK(!SampleRoadDisplay(line, section, 0.25, {.StationsMost = 128, .PoseQueriesMost = 128}),
        "local Float32 accuracy alone cannot authorize inaccurate final source-frame storage");
  return Report();
}
