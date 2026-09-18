#include "TerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "TileGeodesy.h"
#include "math/RenderFrame.h"

namespace outshine::Generators {

TerrainMesh BuildTerrainMesh(const Patchwork &candidate,
                             const TangentFrame &frame,
                             TerrainPageLayout layout,
                             int minimumZoom) {
  TerrainMesh result;
  if (!layout.Valid()) { return result; }
  result.TallestM = -std::numeric_limits<double>::max();
  result.LowestM = std::numeric_limits<double>::max();
  for (const Sheet &sheet : candidate.Sheets) {
    if (sheet.Side != layout.Side || (!sheet.Virtual && sheet.Postings < 2) ||
        sheet.Tile.Zoom < minimumZoom || sheet.Nodes.size() != layout.NodeCount()) {
      continue;
    }
    const auto first = static_cast<uint32_t>(result.PositionsM.size() / 3u);
    for (int row = 0; row < layout.Side; ++row) {
      const double rowFraction = layout.FractionAt(sheet, row);
      for (int column = 0; column < layout.Side; ++column) {
        const double columnFraction = layout.FractionAt(sheet, column);
        const Ground::Geo geo =
            Ground::TileFracToGeo({.X = static_cast<double>(sheet.Tile.X) + columnFraction,
                                   .Y = static_cast<double>(sheet.Tile.Y) + rowFraction},
                                  sheet.Tile.Zoom);
        const auto heightM = static_cast<double>(sheet.Nodes[layout.NodeAt(column, row)]);
        const EastNorthUp placed = frame.Place(
            {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = heightM});
        result.PositionsM.push_back(static_cast<float>(placed.EastM));
        result.PositionsM.push_back(static_cast<float>(placed.UpM));
        result.PositionsM.push_back(static_cast<float>(RenderFrame::ZOfNorth(placed.NorthM)));
        if (heightM > result.TallestM) {
          result.TallestM = heightM;
          result.TallestDistanceM = std::hypot(placed.EastM, placed.NorthM);
        }
        result.LowestM = std::min(result.LowestM, heightM);
      }
    }
    const auto indexAt = [first, side = layout.Side](int column, int row) {
      return first + static_cast<uint32_t>(row) * static_cast<uint32_t>(side) +
             static_cast<uint32_t>(column);
    };
    for (int row = 0; row + 1 < layout.Side; ++row) {
      for (int column = 0; column + 1 < layout.Side; ++column) {
        result.Indices.insert(result.Indices.end(),
                              {indexAt(column, row),
                               indexAt(column, row + 1),
                               indexAt(column + 1, row + 1),
                               indexAt(column, row),
                               indexAt(column + 1, row + 1),
                               indexAt(column + 1, row)});
      }
    }
  }
  if (result.PositionsM.empty()) {
    result.TallestM = 0.0;
    result.LowestM = 0.0;
  }
  return result;
}

}
