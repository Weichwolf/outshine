#include "SurfaceTile.h"
#include "TileId.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
namespace {

SurfaceCoverageInterval TileInterval(Data::TileId tile) {
  uint64_t morton = 0;
  for (int level = tile.Zoom; level > 0; --level) {
    const auto bit = static_cast<unsigned>(level - 1);
    morton = morton * 4 + ((tile.X >> bit) & 1u) + 2 * uint64_t{(tile.Y >> bit) & 1u};
  }
  const auto shift = static_cast<unsigned>(2 * (Data::TileId::MaximumZoom - tile.Zoom));
  return {.Begin = morton << shift, .End = (morton + 1) << shift};
}
}

std::optional<std::vector<SurfaceCoverageInterval>>
SurfaceCoverage(std::span<const SurfaceTile> tiles) {
  std::vector<SurfaceCoverageInterval> intervals;
  intervals.reserve(tiles.size());
  for (const auto &tile : tiles) {
    const auto coverage = tile.Coverage;
    if (coverage.Zoom < 0 || coverage.Zoom > Data::TileId::MaximumZoom) { return std::nullopt; }
    const uint32_t side = uint32_t{1} << static_cast<unsigned>(coverage.Zoom);
    if (coverage.X >= side || coverage.Y >= side) { return std::nullopt; }
    intervals.push_back(TileInterval(coverage));
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

}
