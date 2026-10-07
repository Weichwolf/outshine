#include "GroundRegionAsset.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <numeric>

namespace outshine {
Box GroundRegionBoundsEcef(const GroundRegionAsset &region) {
  Box bounds;
  const TangentFrame frame = TangentFrame::At(region.Anchor);
  const auto cover = [&](const Vec3 &local) {
    bounds.Cover(frame.OriginEcef() + frame.EastEcef() * local[0] + frame.UpEcef() * local[1] -
                 frame.NorthEcef() * local[2]);
  };
  for (int part = 0; part < region.Surfaces.parts(); ++part) {
    const auto positions = region.Surfaces.positionsOf(part);
    const auto &placement = region.Surfaces.placementOf(part);
    for (size_t at = 0; at + 2 < positions.size(); at += 3) {
      cover(placement.TransformPoint({{positions[at], positions[at + 1], positions[at + 2]}}));
    }
  }
  for (const Sheet &sheet : region.Terrain.Sheets) {
    const auto geographic = Ground::TileBounds(sheet.Tile);
    const auto [low, high] = std::ranges::minmax(sheet.Nodes);
    Vec3 centre;
    GeoToEcef({.LongitudeDeg = std::midpoint(geographic.MinLonDeg, geographic.MaxLonDeg),
               .LatitudeDeg = std::midpoint(geographic.MinLatDeg, geographic.MaxLatDeg),
               .HeightM = std::midpoint(static_cast<double>(low), static_cast<double>(high))},
              centre);
    constexpr double derivativeBoundM = 6400000;
    const double radius = (derivativeBoundM + std::max(std::abs(low), std::abs(high))) *
                              (geographic.MaxLonDeg - geographic.MinLonDeg + geographic.MaxLatDeg -
                               geographic.MinLatDeg) *
                              std::numbers::pi / kDegPerTurn +
                          (static_cast<double>(high) - low) * 0.5;
    Box page;
    for (size_t axis = 0; axis < 3; ++axis) {
      page.Min[axis] = centre[axis] - radius;
      page.Max[axis] = centre[axis] + radius;
    }
    bounds.Cover(page);
  }
  if (region.Network) { bounds.Cover(region.Network->BoundsEcef()); }
  if (bounds.Empty()) { bounds.Cover(frame.OriginEcef()); }
  return bounds;
}
}
