#include "TerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "TileGeodesy.h"
#include "math/RenderFrame.h"

namespace outshine::Generators {
namespace {
bool ValidSheet(const Sheet &sheet, TerrainPageLayout layout, int minimumZoom) {
  return layout.Valid() && sheet.Side == layout.Side && (sheet.Virtual || sheet.Postings >= 2) &&
         sheet.Tile.Zoom >= minimumZoom && sheet.Nodes.size() == layout.NodeCount();
}

EastNorthUp NodePosition(
    const Sheet &sheet, const TangentFrame &frame, TerrainPageLayout layout, int column, int row) {
  const Ground::Geo geo = Ground::TileFracToGeo(
      {.X = static_cast<double>(sheet.Tile.X) + layout.FractionAt(sheet, column),
       .Y = static_cast<double>(sheet.Tile.Y) + layout.FractionAt(sheet, row)},
      sheet.Tile.Zoom);
  return frame.ToLocalPosition({.LongitudeDeg = geo.LongitudeDeg,
                                .LatitudeDeg = geo.LatitudeDeg,
                                .HeightM = sheet.Nodes[layout.NodeAt(column, row)]});
}
}

void SurveyTerrainSheet(TerrainSurvey &result,
                        const Sheet &sheet,
                        const TangentFrame &frame,
                        TerrainPageLayout layout,
                        size_t probeStride) {
  if (probeStride == 0 || !ValidSheet(sheet, layout, 0)) { return; }
  for (int row = 0; row < layout.Side; ++row) {
    for (int column = 0; column < layout.Side; ++column) {
      const double heightM = sheet.Nodes[layout.NodeAt(column, row)];
      const bool first = result.Vertices == 0;
      const bool tallest = first || heightM > result.TallestM;
      const bool probe = result.Vertices % probeStride == 0;
      if (tallest || probe) {
        const EastNorthUp placed = NodePosition(sheet, frame, layout, column, row);
        if (probe) {
          result.ProbePositionsM.push_back(
              {{static_cast<float>(placed.EastM),
                static_cast<float>(placed.UpM),
                static_cast<float>(RenderFrame::ZOfNorth(placed.NorthM))}});
        }
        if (tallest) {
          result.TallestM = heightM;
          result.TallestDistanceM = std::hypot(placed.EastM, placed.NorthM);
        }
      }
      result.LowestM = first ? heightM : std::min(result.LowestM, heightM);
      ++result.Vertices;
    }
  }
}

void AppendTerrainMeshSheet(TerrainMesh &result,
                            const Sheet &sheet,
                            const TangentFrame &frame,
                            TerrainPageLayout layout,
                            int minimumZoom) {
  if (!ValidSheet(sheet, layout, minimumZoom)) { return; }
  const auto first = static_cast<uint32_t>(result.PositionsM.size() / 3u);
  for (int row = 0; row < layout.Side; ++row) {
    for (int column = 0; column < layout.Side; ++column) {
      const auto heightM = static_cast<double>(sheet.Nodes[layout.NodeAt(column, row)]);
      const EastNorthUp placed = NodePosition(sheet, frame, layout, column, row);
      result.PositionsM.push_back(static_cast<float>(placed.EastM));
      result.PositionsM.push_back(static_cast<float>(placed.UpM));
      result.PositionsM.push_back(static_cast<float>(RenderFrame::ZOfNorth(placed.NorthM)));
      if (result.PositionsM.size() == 3u || heightM > result.TallestM) {
        result.TallestM = heightM;
        result.TallestDistanceM = std::hypot(placed.EastM, placed.NorthM);
      }
      if (result.PositionsM.size() == 3u) {
        result.LowestM = heightM;
      } else {
        result.LowestM = std::min(result.LowestM, heightM);
      }
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

TerrainMesh BuildTerrainMesh(const Patchwork &candidate,
                             const TangentFrame &frame,
                             TerrainPageLayout layout,
                             int minimumZoom) {
  TerrainMesh result;
  for (const Sheet &sheet : candidate.Sheets) {
    AppendTerrainMeshSheet(result, sheet, frame, layout, minimumZoom);
  }
  return result;
}

}
