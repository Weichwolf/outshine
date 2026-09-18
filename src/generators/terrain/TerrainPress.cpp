#include "TerrainPress.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "ChunkSurface.h"
#include "TileGeodesy.h"
#include "TerrainGrid.h"
#include "math/Vec3.h"

namespace outshine::Generators {
namespace {

[[nodiscard]] double FractionOf(int node, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(node, postings, side)) /
         static_cast<double>(postings - 1u);
}

[[nodiscard]] double NodeFraction(const Sheet &sheet, int node, int side) {
  if (sheet.Virtual || sheet.SourceZoom >= 0) {
    return static_cast<double>(node) / static_cast<double>(side - 1);
  }
  if (node < 0) { return -FractionOf(1, sheet.Postings, side); }
  if (node >= side) { return 2.0 - FractionOf(side - 2, sheet.Postings, side); }
  return FractionOf(node, sheet.Postings, side);
}

[[nodiscard]] size_t PageNode(int column, int row, TerrainPageLayout layout) {
  const auto pageSide = static_cast<size_t>(layout.Side) + 2u * static_cast<size_t>(layout.Halo);
  return static_cast<size_t>(row + layout.Halo) * pageSide +
         static_cast<size_t>(column + layout.Halo);
}

}

PressedTerrain PressTerrain(std::span<const Yields> yields,
                            Patchwork &candidate,
                            const TangentFrame &frame,
                            TerrainPageLayout layout,
                            double mostEarthworkM) {
  if (yields.empty() || layout.Side < 2 || layout.Halo < 0) { return {}; }
  const int64_t pageSideWide =
      static_cast<int64_t>(layout.Side) + 2 * static_cast<int64_t>(layout.Halo);
  if (pageSideWide > static_cast<int64_t>(std::numeric_limits<int>::max())) { return {}; }
  const auto pageSide = static_cast<size_t>(pageSideWide);
  if (pageSide > std::numeric_limits<size_t>::max() / pageSide) { return {}; }
  const size_t pageNodes = pageSide * pageSide;
  std::vector<EastNorth> positions;
  std::vector<double> heights;
  std::vector<std::pair<size_t, size_t>> sources;
  for (size_t sheetAt = 0; sheetAt < candidate.Sheets.size(); ++sheetAt) {
    const Sheet &sheet = candidate.Sheets[sheetAt];
    if (sheet.Side != layout.Side || (!sheet.Virtual && sheet.Postings < 2) ||
        sheet.Nodes.size() != pageNodes) {
      continue;
    }
    for (int row = -layout.Halo; row < layout.Side + layout.Halo; ++row) {
      const double rowFraction = NodeFraction(sheet, row, layout.Side);
      for (int column = -layout.Halo; column < layout.Side + layout.Halo; ++column) {
        const double columnFraction = NodeFraction(sheet, column, layout.Side);
        const Ground::Geo geo =
            Ground::TileFracToGeo({.X = static_cast<double>(sheet.Tile.X) + columnFraction,
                                   .Y = static_cast<double>(sheet.Tile.Y) + rowFraction},
                                  sheet.Tile.Zoom);
        const size_t node = PageNode(column, row, layout);
        const EastNorthUp placed = frame.Place({.LongitudeDeg = geo.LongitudeDeg,
                                                .LatitudeDeg = geo.LatitudeDeg,
                                                .HeightM = static_cast<double>(sheet.Nodes[node])});
        positions.push_back({.EastM = placed.EastM, .NorthM = placed.NorthM});
        heights.push_back(placed.UpM);
        sources.emplace_back(sheetAt, node);
      }
    }
  }
  std::vector<double> previous(heights);
  const Pressed pressed = PressPoints(yields, positions, heights, mostEarthworkM);
  PressedTerrain result{.Nodes = pressed.Moved,
                        .Structures = pressed.Structures,
                        .Held = pressed.Held,
                        .DeepestM = 0.0,
                        .RaisedM = 0.0,
                        .Pads = {},
                        .Corridors = {}};
  if (pressed.Moved == 0) { return result; }
  const Vec3 &origin = frame.OriginEcef();
  const Vec3 &east = frame.EastEcef();
  const Vec3 &north = frame.NorthEcef();
  const Vec3 &up = frame.UpEcef();
  for (size_t point = 0; point < heights.size(); ++point) {
    if (heights[point] == previous[point]) { continue; }
    result.DeepestM = std::max(result.DeepestM, previous[point] - heights[point]);
    result.RaisedM = std::max(result.RaisedM, heights[point] - previous[point]);
    const double e = positions[point].EastM;
    const double n = positions[point].NorthM;
    Vec3 ecef;
    for (int axis = 0; axis < 3; ++axis) {
      ecef[axis] = origin[axis] + e * east[axis] + n * north[axis] + heights[point] * up[axis];
    }
    const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
    candidate.Sheets[sources[point].first].Nodes[sources[point].second] =
        static_cast<float>(geo.HeightM);
  }
  std::vector<double> written(heights.size());
  for (size_t point = 0; point < heights.size(); ++point) {
    const Sheet &sheet = candidate.Sheets[sources[point].first];
    const int column = static_cast<int>(sources[point].second % pageSide) - layout.Halo;
    const int row = static_cast<int>(sources[point].second / pageSide) - layout.Halo;
    const Ground::Geo geo = Ground::TileFracToGeo(
        {.X = static_cast<double>(sheet.Tile.X) + NodeFraction(sheet, column, layout.Side),
         .Y = static_cast<double>(sheet.Tile.Y) + NodeFraction(sheet, row, layout.Side)},
        sheet.Tile.Zoom);
    written[point] =
        frame
            .Place({.LongitudeDeg = geo.LongitudeDeg,
                    .LatitudeDeg = geo.LatitudeDeg,
                    .HeightM = static_cast<double>(sheet.Nodes[sources[point].second])})
            .UpM;
  }
  const Heights finalHeights{.WrittenM = written, .WasM = previous};
  result.Pads = FloorsOf(yields, pressed, Stamp::Pad, positions, finalHeights);
  result.Corridors = FloorsOf(yields, pressed, Stamp::Corridor, positions, finalHeights);
  return result;
}

}
