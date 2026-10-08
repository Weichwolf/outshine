#include "WaterAsset.h"
#include "TileGeodesy.h"
#include "Earth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Generators {
namespace {

bool InsideRing(std::span<const double> points,
                const WaterAsset::SurfaceRing &ring,
                LongitudeLatitude at) noexcept {
  if (ring.PointCount < 3) { return false; }
  constexpr double kLongitudePeriodDeg = 360.0;
  constexpr double kBoundaryWithinDeg = 1e-12;
  const auto point = [&](size_t vertex) {
    const size_t index = (static_cast<size_t>(ring.FirstPoint) + vertex) * 2;
    return std::array{std::remainder(points[index + 1] - at.LongitudeDeg, kLongitudePeriodDeg),
                      points[index] - at.LatitudeDeg};
  };
  bool inside = false;
  auto previous = point(ring.PointCount - 1);
  for (size_t vertex = 0; vertex < ring.PointCount; ++vertex) {
    const auto current = point(vertex);
    const double cross = previous[0] * current[1] - current[0] * previous[1];
    const double span = std::hypot(current[0] - previous[0], current[1] - previous[1]);
    if (std::abs(cross) <= kBoundaryWithinDeg * span &&
        std::min(previous[0], current[0]) <= kBoundaryWithinDeg &&
        std::max(previous[0], current[0]) >= -kBoundaryWithinDeg &&
        std::min(previous[1], current[1]) <= kBoundaryWithinDeg &&
        std::max(previous[1], current[1]) >= -kBoundaryWithinDeg) {
      return true;
    }
    if ((previous[1] > 0) != (current[1] > 0) &&
        previous[0] + (current[0] - previous[0]) * -previous[1] / (current[1] - previous[1]) > 0) {
      inside = !inside;
    }
    previous = current;
  }
  return inside;
}

bool Contains(std::span<const double> points,
              std::span<const WaterAsset::SurfaceRing> rings,
              LongitudeLatitude at) noexcept {
  return !rings.empty() && InsideRing(points, rings.front(), at) &&
         std::ranges::none_of(rings.subspan(1),
                              [&](const auto &ring) { return InsideRing(points, ring, at); });
}

}

std::optional<float> WaterAsset::LevelAt(LongitudeLatitude at) const noexcept {
  const auto &tiles = Contents().Tiles;
  std::optional<float> level;
  int previousZoom = -1;
  std::optional<::outshine::Data::TileId> located;
  for (size_t index = 0; index < tiles.size(); ++index) {
    const auto &entry = tiles[index];
    if (entry.Zoom != previousZoom) {
      located = Ground::TileIndex::Of(
                    {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, entry.Zoom)
                    .Tile();
      previousZoom = entry.Zoom;
    }
    if (!located) { continue; }
    const auto across = static_cast<int64_t>(uint64_t{1} << static_cast<unsigned>(entry.Zoom));
    const int64_t deltaX = std::abs(static_cast<int64_t>(entry.X) - located->X);
    if (std::min(deltaX, across - deltaX) > 1 ||
        std::abs(static_cast<int64_t>(entry.Y) - located->Y) > 1) {
      continue;
    }
    for (const Surface &surface : OfTile(static_cast<int>(index))) {
      if ((!level || surface.LevelM > *level) && Contains(Points(), RingsOf(surface), at)) {
        level = surface.LevelM;
      }
    }
  }
  return level;
}
}
