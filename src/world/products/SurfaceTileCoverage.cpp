#include "SurfaceTile.h"
#include "TileId.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <iterator>
#include <span>
#include <vector>

namespace outshine {
namespace {

std::optional<SurfaceCoverageInterval> TileInterval(Data::TileId tile) {
  if (tile.Zoom < 0 || tile.Zoom > Data::TileId::MaximumZoom) { return std::nullopt; }
  const uint32_t side = uint32_t{1} << static_cast<unsigned>(tile.Zoom);
  if (tile.X >= side || tile.Y >= side) { return std::nullopt; }
  uint64_t morton = 0;
  for (int level = tile.Zoom; level > 0; --level) {
    const auto bit = static_cast<unsigned>(level - 1);
    morton = morton * 4 + ((tile.X >> bit) & 1u) + 2 * uint64_t{(tile.Y >> bit) & 1u};
  }
  const auto shift = static_cast<unsigned>(2 * (Data::TileId::MaximumZoom - tile.Zoom));
  return SurfaceCoverageInterval{.Begin = morton << shift, .End = (morton + 1) << shift};
}
}

std::optional<std::vector<SurfaceCoverageInterval>>
SurfaceCoverage(std::span<const SurfaceTile> tiles) {
  std::vector<SurfaceCoverageInterval> intervals;
  intervals.reserve(tiles.size());
  for (const auto &tile : tiles) {
    const auto interval = TileInterval(tile.Coverage);
    if (!interval) { return std::nullopt; }
    intervals.push_back(*interval);
  }
  std::ranges::sort(intervals, {}, &SurfaceCoverageInterval::Begin);
  size_t written = 0;
  for (const auto [begin, end] : intervals) {
    if (written > 0 && begin <= intervals[written - 1].End) {
      if (begin < intervals[written - 1].End) { return std::nullopt; }
      intervals[written - 1].End = end;
    } else {
      intervals[written++] = {.Begin = begin, .End = end};
    }
  }
  intervals.resize(written);
  return intervals;
}

bool SurfaceCoversTile(std::span<const SurfaceCoverageInterval> coverage, Data::TileId tile) {
  const auto footprint = TileInterval(tile);
  if (!footprint) { return false; }
  const auto next =
      std::ranges::upper_bound(coverage, footprint->Begin, {}, &SurfaceCoverageInterval::Begin);
  return next != coverage.begin() && std::prev(next)->End >= footprint->End;
}

}
