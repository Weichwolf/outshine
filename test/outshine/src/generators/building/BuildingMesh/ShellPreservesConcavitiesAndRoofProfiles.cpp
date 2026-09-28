#include "src/generators/building/BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace {
struct LocalPoint {
  double East;
  double North;
  double Up;
};

LocalPoint Local(const outshine::StoredVertex &vertex, const outshine::EnuAxes &axes) {
  LocalPoint point{};
  for (int axis = 0; axis < 3; ++axis) {
    point.East += static_cast<double>(vertex.pos[axis]) * axes.East[axis];
    point.North += static_cast<double>(vertex.pos[axis]) * axes.North[axis];
    point.Up += static_cast<double>(vertex.pos[axis]) * axes.Up[axis];
  }
  return point;
}

double Side(const LocalPoint &a, const LocalPoint &b, const LocalPoint &point) {
  return (b.East - a.East) * (point.North - a.North) - (b.North - a.North) * (point.East - a.East);
}

bool RoofCovers(const outshine::Raised &mesh, const outshine::EnuAxes &axes, LocalPoint point) {
  for (size_t at = 0; at + 2 < mesh.RoofRun.size(); at += 3) {
    const auto a = Local(mesh.RoofCorners[mesh.RoofRun[at]], axes);
    const auto b = Local(mesh.RoofCorners[mesh.RoofRun[at + 1]], axes);
    const auto c = Local(mesh.RoofCorners[mesh.RoofRun[at + 2]], axes);
    if (std::abs(Side(a, b, c)) < 1e-6) { continue; }
    const std::array signs{Side(a, b, point), Side(b, c, point), Side(c, a, point)};
    if (*std::ranges::min_element(signs) >= -1e-6 || *std::ranges::max_element(signs) <= 1e-6) {
      return true;
    }
  }
  return false;
}

double LowestPoint(const outshine::Raised &mesh, const outshine::EnuAxes &axes) {
  double lowest = 1e30;
  for (const auto &vertex : mesh.WallCorners) { lowest = std::min(lowest, Local(vertex, axes).Up); }
  for (const auto &vertex : mesh.RoofCorners) { lowest = std::min(lowest, Local(vertex, axes).Up); }
  return lowest;
}

bool SlopedRoof(const outshine::Raised &mesh, const outshine::EnuAxes &axes) {
  for (size_t at = 0; at + 2 < mesh.RoofRun.size(); at += 3) {
    const auto a = Local(mesh.RoofCorners[mesh.RoofRun[at]], axes);
    const auto b = Local(mesh.RoofCorners[mesh.RoofRun[at + 1]], axes);
    const auto c = Local(mesh.RoofCorners[mesh.RoofRun[at + 2]], axes);
    if (std::abs(Side(a, b, c)) > 0.01 &&
        std::max({a.Up, b.Up, c.Up}) - std::min({a.Up, b.Up, c.Up}) > 0.5) {
      return true;
    }
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 9, .LatitudeDeg = 47};
  const auto axes =
      EnuAxesEcef({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg});
  Vec3 anchor;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 0}, anchor);
  const double metresPerLon = kMPerDegLon * std::cos(47 * kDeg2Rad);
  const std::array<LocalPoint, 6> shape{
      {{0, 0, 0}, {40, 0, 0}, {40, 10, 0}, {10, 10, 0}, {10, 40, 0}, {0, 40, 0}}};
  std::array<double, 12> concave{};
  for (size_t at = 0; at < shape.size(); ++at) {
    concave[2 * at] = 47 + shape[at].North / kMPerDegLat;
    concave[2 * at + 1] = 9 + shape[at].East / metresPerLon;
  }
  const std::array<double, 4> cornerHeights{0, 1, 2, 1};
  const std::array<double, 8> rectangle{47,
                                        9,
                                        47,
                                        9 + 12 / metresPerLon,
                                        47 + 16 / kMPerDegLat,
                                        9 + 12 / metresPerLon,
                                        47 + 16 / kMPerDegLat,
                                        9};
  Raised boxed;
  boxed.RoofCorners.resize(4);
  const std::array<LocalPoint, 4> box{{{0, 0, 9}, {40, 0, 9}, {40, 40, 9}, {0, 40, 9}}};
  for (size_t at = 0; at < box.size(); ++at) {
    for (int axis = 0; axis < 3; ++axis) {
      boxed.RoofCorners[at].pos[axis] =
          static_cast<float>(box[at].East * axes.East[axis] + box[at].North * axes.North[axis] +
                             box[at].Up * axes.Up[axis]);
    }
  }
  boxed.RoofRun = {0, 1, 2, 0, 2, 3};
  CHECK(RoofCovers(boxed, axes, {35, 35, 0}),
        "independent bounding-box cap detects an incorrectly filled footprint notch");
  CHECK(!SlopedRoof(boxed, axes), "independent flat box lacks the pitched-profile signature");
  Generators::BuildingMesh mesher;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    StructurePlan plan;
    plan.AnchorEcef = anchor;
    plan.RingLatLon = concave;
    plan.HeightM = 9;
    plan.HeightMeasured = true;
    plan.PitchedShare = 0;
    plan.Coarseness = detail;
    auto scratch = mesher.Scratch();
    Raised mesh;
    CHECK(mesher.Mesh(plan, *scratch, mesh).has_value(),
          "concave plan meshes at each explicit level");
    CHECK(RoofCovers(mesh, axes, {5, 25, 0}), "roof covers the actual north arm");
    if (detail != LevelOfDetail::Massed) {
      CHECK(!RoofCovers(mesh, axes, {35, 35, 0}),
            "Fine and Shell leave the analytic footprint notch empty");
    }
    plan.RingLatLon = rectangle;
    plan.CornerAslM = cornerHeights;
    plan.SeatAslM = 2;
    plan.FootAslM = 0;
    plan.PitchedShare = 1;
    scratch = mesher.Scratch();
    mesh = {};
    CHECK(mesher.Mesh(plan, *scratch, mesh).has_value(),
          "pitched plan meshes at each explicit level");
    CHECK(LowestPoint(mesh, axes) <= 0.0,
          "foundation reaches the lowest declared terrain level despite the two-metre slope");
    if (detail == LevelOfDetail::Massed) {
      CHECK(!SlopedRoof(mesh, axes), "coarse flat cap is an effective missing-profile control");
    } else {
      CHECK(SlopedRoof(mesh, axes), "Fine and Shell retain a genuinely sloping roof surface");
    }
  }
  return Report();
}
