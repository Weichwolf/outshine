#include "WaterBasinStamps.h"
#include "WaterAsset.h"
#include "WaterSurfaceMesh.h"
#include "EarthworkDeformation.h"
#include "GroundMesher.h"
#include "TangentFrame.h"
#include "Check.h"
#include <cstddef>
#include <array>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::vector<EarthworkStamp> Stamps(bool split) {
  const WaterAsset source({.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 999}},
                           .Rings = {{.FirstPoint = 0, .PointCount = 4}},
                           .Courses = {},
                           .Levels = {},
                           .Points = {-.001, -.001, -.001, .001, .001, .001, .001, -.001},
                           .Tiles = {},
                           .Sources = {},
                           .Inputs = {}});
  std::vector<Vec3> vertices{
      {{-100, -100, 9}}, {{100, -100, 11}}, {{100, 100, 11}}, {{-100, 100, 9}}};
  std::vector<WaterSurfaceMesh::Face> faces{{.Vertices = {0, 1, 2}}, {.Vertices = {0, 2, 3}}};
  if (split) {
    vertices.push_back({{0, 0, 10}});
    faces = {{.Vertices = {0, 1, 4}},
             {.Vertices = {1, 2, 4}},
             {.Vertices = {2, 3, 4}},
             {.Vertices = {3, 0, 4}}};
  }
  auto mesh = WaterSurfaceMesh::Build({}, std::move(vertices), std::move(faces));
  CHECK(mesh, "a sloping canonical surface is valid");
  if (!mesh) { return {}; }
  auto water = source.WithSurfaces({.Mesh = std::move(*mesh),
                                    .Rejections = {},
                                    .FaceSourceOffsets = {},
                                    .FaceSources = {},
                                    .ProfileDeclines = {}});
  CHECK(water, "canonical facets cover the source body");
  std::vector<EarthworkStamp> stamps;
  if (water) { AppendWaterBasinStamps(*water, TangentFrame::At({}), 2, stamps); }
  return stamps;
}
}

int main() {
  const auto two = Stamps(false);
  const auto four = Stamps(true);
  CHECK(two.size() == 2 && four.size() == 4, "subdivision changes only the facet count");
  std::vector<EastNorth> points;
  for (const double east : {-80., -40., 0., 40., 80.}) {
    for (const double north : {-80., -40., 0., 40., 80.}) {
      points.push_back({.EastM = east, .NorthM = north});
    }
  }
  points.push_back({.EastM = 150, .NorthM = 0});
  std::vector<double> first(points.size(), 30);
  std::vector<double> refined(points.size(), 30);
  const auto result = ApplyEarthworkStamps(two, points, first, 40);
  const auto refinement = ApplyEarthworkStamps(four, points, refined, 40);
  CHECK(result.Moved == 25 && refinement.Moved == 25,
        "both subdivisions excavate all wet samples and leave dry terrain alone");
  for (size_t i = 0; i < points.size() - 1; ++i) {
    const double expected = 8 + .01 * points[i].EastM;
    CHECK(std::abs(first[i] - expected) < 1e-10 && std::abs(refined[i] - expected) < 1e-10,
          "internal facet edges never become raised banks or expose terrain through water");
  }
  CHECK(first.back() == 30 && refined.back() == 30,
        "terrain outside the water footprint is retained for its natural shore intersection");
  auto wrong = two;
  for (auto &stamp : wrong) { stamp.ApronM = 2 / kBatterRise; }
  std::vector<double> control(points.size(), 30);
  (void)ApplyEarthworkStamps(wrong, points, control, 40);
  CHECK(std::abs(control[12] - first[12]) > 1,
        "the former per-facet shore recipe reproduces the observed internal-bank defect");
  return Report();
}
