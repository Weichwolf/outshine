#include "TerrainPress.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ratio>
#include <span>
#include <utility>
#include <vector>

#include "TileGeodesy.h"
#include "TerrainGrid.h"
#include "math/Vec3.h"

namespace outshine::Generators {
PressedTerrain PressTerrain(std::span<const EarthworkStamp> yields,
                            Patchwork &candidate,
                            const TangentFrame &frame,
                            TerrainPageLayout layout,
                            double mostEarthworkM) {
  if (yields.empty() || !layout.Valid()) { return {}; }
  const auto began = std::chrono::steady_clock::now();
  const size_t pageSide = layout.PageSide();
  std::vector<EastNorth> positions;
  std::vector<double> heights;
  std::vector<std::pair<size_t, size_t>> sources;
  for (size_t sheetAt = 0; sheetAt < candidate.Sheets.size(); ++sheetAt) {
    const Sheet &sheet = candidate.Sheets[sheetAt];
    if (sheet.Side != layout.Side || (!sheet.Virtual && sheet.Postings < 2) ||
        sheet.Nodes.size() != layout.NodeCount()) {
      continue;
    }
    for (int row = -layout.Halo; row < layout.Side + layout.Halo; ++row) {
      const double rowFraction = layout.FractionAt(sheet, row);
      for (int column = -layout.Halo; column < layout.Side + layout.Halo; ++column) {
        const double columnFraction = layout.FractionAt(sheet, column);
        const Ground::Geo geo =
            Ground::TileFracToGeo({.X = static_cast<double>(sheet.Tile.X) + columnFraction,
                                   .Y = static_cast<double>(sheet.Tile.Y) + rowFraction},
                                  sheet.Tile.Zoom);
        const size_t node = layout.NodeAt(column, row);
        const EastNorthUp placed =
            frame.ToLocalPosition({.LongitudeDeg = geo.LongitudeDeg,
                                   .LatitudeDeg = geo.LatitudeDeg,
                                   .HeightM = static_cast<double>(sheet.Nodes[node])});
        positions.push_back({.EastM = placed.EastM, .NorthM = placed.NorthM});
        heights.push_back(placed.UpM);
        sources.emplace_back(sheetAt, node);
      }
    }
  }
  std::vector<double> previous(heights);
  const auto gathered = std::chrono::steady_clock::now();
  const EarthworkPressResult pressed =
      ApplyEarthworkStamps(yields, positions, heights, mostEarthworkM);
  const auto decided = std::chrono::steady_clock::now();
  PressedTerrain result{
      .Nodes = pressed.Moved,
      .Structures = pressed.Structures,
      .Held = pressed.Held,
      .DeepestM = 0.0,
      .RaisedM = 0.0,
      .Pads = {},
      .Corridors = {},
      .GatherMs = std::chrono::duration<double, std::milli>(gathered - began).count(),
      .DecideMs = std::chrono::duration<double, std::milli>(decided - gathered).count(),
      .BucketMs = pressed.BucketMs,
      .RejectMs = pressed.RejectMs,
      .ApplyMs = pressed.ApplyMs};
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
        {.X = static_cast<double>(sheet.Tile.X) + layout.FractionAt(sheet, column),
         .Y = static_cast<double>(sheet.Tile.Y) + layout.FractionAt(sheet, row)},
        sheet.Tile.Zoom);
    written[point] =
        frame
            .ToLocalPosition({.LongitudeDeg = geo.LongitudeDeg,
                              .LatitudeDeg = geo.LatitudeDeg,
                              .HeightM = static_cast<double>(sheet.Nodes[sources[point].second])})
            .UpM;
  }
  const auto writtenAt = std::chrono::steady_clock::now();
  const EarthworkHeightView finalHeights{.WrittenM = written, .WasM = previous};
  result.Pads =
      MeasureEarthworkEffect(yields, pressed, EarthworkKind::Pad, positions, finalHeights);
  result.Corridors =
      MeasureEarthworkEffect(yields, pressed, EarthworkKind::Corridor, positions, finalHeights);
  const auto finished = std::chrono::steady_clock::now();
  result.WriteMs = std::chrono::duration<double, std::milli>(writtenAt - decided).count();
  result.FloorsMs = std::chrono::duration<double, std::milli>(finished - writtenAt).count();
  return result;
}

}
