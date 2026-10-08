#include "WaterField.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
struct CoordinateRange {
  size_t First = 0, End = 0, Packed = 0;
};

std::vector<CoordinateRange> CoordinateRanges(std::span<const WaterField::SurfaceRing> rings,
                                              std::span<const WaterField::Course> courses) {
  std::vector<CoordinateRange> ranges;
  ranges.reserve(rings.size() + courses.size());
  const auto append = [&](const auto &record) {
    ranges.push_back({.First = record.FirstPoint,
                      .End = static_cast<size_t>(record.FirstPoint) + record.PointCount});
  };
  for (const auto &ring : rings) { append(ring); }
  for (const auto &course : courses) { append(course); }
  std::ranges::sort(ranges, {}, &CoordinateRange::First);
  size_t kept = 0;
  for (const auto range : ranges) {
    if (kept != 0 && range.First <= ranges[kept - 1].End) {
      ranges[kept - 1].End = std::max(ranges[kept - 1].End, range.End);
    } else {
      ranges[kept++] = range;
    }
  }
  ranges.resize(kept);
  size_t packed = 0;
  for (auto &range : ranges) {
    range.Packed = packed;
    packed += range.End - range.First;
  }
  assert(packed <= std::numeric_limits<uint32_t>::max());
  return ranges;
}

uint32_t PackedPoint(std::span<const CoordinateRange> ranges, uint32_t point) {
  auto range = std::ranges::upper_bound(ranges, point, {}, &CoordinateRange::First);
  assert(range != ranges.begin());
  --range;
  assert(point < range->End);
  return static_cast<uint32_t>(range->Packed + point - range->First);
}
}

WaterAsset WaterField::Asset(const OsmField &source) const {
  const AssetInputs inputs{.Origin = source.OriginToken(),
                           .Generation = source.Generation(),
                           .Features = source.Features().size(),
                           .Tiles = source.Tiles().size(),
                           .Points = source.Points().size()};
  if (Asset_ && AssetInputs_ == inputs) { return *Asset_; }
  WaterAsset::Data data{.Surfaces = Surfaces_,
                        .Rings = SurfaceRings_,
                        .Courses = Courses_,
                        .Levels = Levels_,
                        .Points = {},
                        .Tiles = {},
                        .ProcessedTiles = static_cast<uint32_t>(Admission_.Takes()),
                        .NoGround = NoGround_,
                        .Outliers = Outliers_,
                        .InvalidBodies = InvalidBodies_,
                        .Status = Ingested(source) ? WaterAsset::Coverage::Complete
                                                   : WaterAsset::Coverage::Partial};
  const auto ranges = CoordinateRanges(data.Rings, data.Courses);
  if (!ranges.empty()) {
    const auto &last = ranges.back();
    data.Points.reserve((last.Packed + last.End - last.First) * 2);
  }
  for (const auto &range : ranges) {
    const auto points = source.Points().subspan(range.First * 2, (range.End - range.First) * 2);
    data.Points.insert(data.Points.end(), points.begin(), points.end());
  }
  for (auto &ring : data.Rings) { ring.FirstPoint = PackedPoint(ranges, ring.FirstPoint); }
  for (auto &course : data.Courses) { course.FirstPoint = PackedPoint(ranges, course.FirstPoint); }
  data.Tiles.reserve(source.Tiles().size());
  for (size_t at = 0; at < source.Tiles().size(); ++at) {
    const auto &tile = source.Tiles()[at];
    const auto surfaces = ByTile_.At(static_cast<uint32_t>(at));
    data.Tiles.push_back({.Zoom = tile.Z,
                          .X = tile.X,
                          .Y = tile.Y,
                          .FirstSurface = surfaces.First,
                          .SurfaceCount = surfaces.Count});
  }
  AssetInputs_ = inputs;
  Asset_.emplace(std::move(data));
  return *Asset_;
}

std::optional<float> WaterField::LevelAt(const OsmField &field, LongitudeLatitude at) const {
  return Asset(field).LevelAt(at);
}
}
