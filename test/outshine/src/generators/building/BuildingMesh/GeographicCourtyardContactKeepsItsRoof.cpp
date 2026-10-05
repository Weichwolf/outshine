#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "RoofSurface.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace {
// OpenFreeMap 20260927, 14/14549/6451, feature 213751370: a courtyard touches its exterior.
constexpr std::array<double, 26> kPoints{
    35.688620346979455, 139.68178510665894, 35.688698772529698, 139.68205869197845,
    35.688681344636308, 139.68207478523254, 35.688685701610019, 139.68209087848663,
    35.688663916739110, 139.68210160732269, 35.688668273713773, 139.68211770057678,
    35.688633417909848, 139.68213379383087, 35.688554992295344, 139.68194603919983,
    35.688520136441937, 139.68182802200317, 35.688672630688181, 139.68196749687195,
    35.688611633024664, 139.68199431896210, 35.688629060933287, 139.68204796314240,
    35.688685701610019, 139.68202114105225};

double Area(std::span<const outshine::EastNorth> ring) {
  double twice = 0;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto &a = ring[at], &b = ring[(at + 1) % ring.size()];
    twice += a.EastM * b.NorthM - b.EastM * a.NorthM;
  }
  return std::abs(twice) * 0.5;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<GeographicRing, 1> holes{{{.First = 9, .Count = 4, .Exterior = false}}};
  StructurePlan plan;
  plan.RingLatLon = std::span(kPoints).first(18);
  plan.RingPointsLatLon = kPoints;
  plan.InnerRings = holes;
  plan.HeightM = 36;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0;
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  for (const auto level : {LevelOfDetail::Fine, LevelOfDetail::Shell}) {
    plan.Coarseness = level;
    Raised output;
    const auto built = mesher.Mesh(plan, *scratch, output);
    CHECK(built && !output.RoofRun.empty(), "geographic courtyard contact retains a complete roof");
    if (!built) { continue; }
    BuildingScratch shapeScratch;
    const auto mass = MassOf(plan.RingLatLon,
                             {.HeightM = 36, .HeightMeasured = true, .PitchedShare = 0},
                             plan.Street,
                             shapeScratch,
                             plan.InnerRings,
                             plan.RingPointsLatLon);
    CHECK(mass && mass->size() == 1 && mass->front().Holes.size() == 1,
          "the actual courtyard is retained as a hole");
    if (!mass || mass->size() != 1 || mass->front().Holes.size() != 1) { continue; }
    const auto &shape = mass->front();
    bool connected = false;
    for (const auto &point : shape.Holes.front()) {
      connected |= std::ranges::any_of(shape.Ring, [&](const auto &outer) {
        return point.EastM == outer.EastM && point.NorthM == outer.NorthM;
      });
    }
    CHECK(connected, "the shared boundary contact has identical coordinates in both rings");
    std::vector<EastNorth> triangles;
    CHECK(RoofSurface::Fill(shape.Ring, shapeScratch, triangles, shape.Holes),
          "the connected courtyard triangulates without omitting its boundary");
    double area = 0;
    for (size_t at = 0; at + 2 < triangles.size(); at += 3) {
      area += Area(std::span(triangles).subspan(at, 3));
    }
    CHECK_NEAR(area,
               Area(shape.Ring) - Area(shape.Holes.front()),
               1.0e-6,
               "m2",
               "roof area equals the footprint with the courtyard removed");
  }
  auto outside = kPoints;
  outside[18] += 0.00001;
  plan.RingPointsLatLon = outside;
  Raised rejected;
  CHECK(!mesher.Mesh(plan, *scratch, rejected),
        "a courtyard extending a metre outside stays invalid");
  CHECK(rejected.WallRun.empty() && rejected.RoofRun.empty(),
        "invalid courtyard publishes no partial geometry");
  return Report();
}
