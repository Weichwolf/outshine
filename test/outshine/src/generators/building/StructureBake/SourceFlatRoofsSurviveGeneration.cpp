#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/BuildingShape.h"
#include "src/generators/building/BuildingScratch.h"
#include "src/generators/building/StructureBake.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

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
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double latitude = 47.0;
  constexpr double longitude = 9.0;
  const double metresPerLon = kMPerDegLon * std::cos(latitude * kDeg2Rad);
  RawTile raw;
  raw.LatLon = {latitude,
                longitude,
                latitude,
                longitude + 12.0 / metresPerLon,
                latitude + 16.0 / kMPerDegLat,
                longitude + 12.0 / metresPerLon,
                latitude + 16.0 / kMPerDegLat,
                longitude};
  const auto cell = StructureCellOf({.MinLonDeg = longitude,
                                     .MinLatDeg = latitude,
                                     .MaxLonDeg = longitude + 0.008,
                                     .MaxLatDeg = latitude + 0.008},
                                    raw.LatLon);
  CHECK(cell.has_value(), "the source building belongs to a native cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 9.0});
  raw.TileSpanM = 1000.0;
  GeoToEcef({.LongitudeDeg = longitude, .LatitudeDeg = latitude, .HeightM = 0.0}, raw.AnchorEcef);
  const auto axes = EnuAxesEcef({.LongitudeDeg = longitude, .LatitudeDeg = latitude});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell}) {
    raw.RequestedDetail = detail;
    for (const int pitched : {0, 1}) {
      raw.Structures.front().Pitched = pitched;
      auto scratch = mesher.Scratch();
      BakedTile mesh;
      CHECK(BakeStructures(raw, *heights, mesher, *scratch, mesh).has_value(),
            "source roof declaration generates a complete building");
      CHECK(!mesh.Built.RoofRun.empty() && SlopedRoof(mesh.Built, axes) == (pitched == 1),
            "actual roof triangles distinguish flat source roofs from pitched control roofs");
    }
  }
  BuildingScratch scratch;
  std::vector<double> round;
  constexpr int corners = 16;
  for (int at = 0; at < corners; ++at) {
    const double angle = static_cast<double>(at) * 2.0 * kPi / corners;
    round.push_back(latitude + 30.0 * std::sin(angle) / kMPerDegLat);
    round.push_back(longitude + 30.0 * std::cos(angle) / metresPerLon);
  }
  const auto parts = MassOf(round, {.HeightM = 24.0, .PitchedShare = 0.0}, {}, scratch);
  CHECK(
      parts && !parts->empty() &&
          std::ranges::all_of(*parts, [](const auto &part) { return part.Roof == RoofKind::Flat; }),
      "explicit flat roofs survive round-footprint and stacked-cap heuristics");
  return Report();
}
