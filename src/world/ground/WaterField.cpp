#include "WaterField.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Ground {

namespace {

constexpr double kLevelPercentile = 0.05;

constexpr double kShoreToleranceM = 5.0;

}

namespace {

constexpr uint32_t kMaxWaterRingPoints = 512;
enum class WaterKind { Ignored, Course, Surface };

WaterKind KindOf(const OsmField &field, const OsmField::Feature &feature, OnLayers layers) {
  if (field.Num(feature, "tunnel", 0.0) > 0.5) { return WaterKind::Ignored; }
  if (feature.Type == 2 && std::cmp_equal(feature.Layer, layers.Line)) { return WaterKind::Course; }
  if (feature.Type == 3 && std::cmp_equal(feature.Layer, layers.Poly)) {
    return WaterKind::Surface;
  }
  return WaterKind::Ignored;
}

bool UsableRing(const OsmField::Ring &ring, WaterKind kind) {
  if (ring.Count > kMaxWaterRingPoints) { return false; }
  if (kind == WaterKind::Course) { return ring.Count >= 2; }
  return kind == WaterKind::Surface && ring.Exterior && ring.Count >= 3;
}

std::span<const double> RingPoints(const OsmField &field, const OsmField::Ring &ring) {
  return field.Points().subspan(static_cast<size_t>(ring.First) * 2,
                                static_cast<size_t>(ring.Count) * 2);
}

LongitudeLatitude PointAt(std::span<const double> points, size_t index) {
  return {.LongitudeDeg = points[index * 2 + 1], .LatitudeDeg = points[index * 2]};
}

bool RingGroundResolved(const GroundQuery &ground, std::span<const double> points) {
  for (size_t at = 0; at < points.size() / 2; ++at) {
    if (ground.At(PointAt(points, at)).Where() == GroundSample::State::Pending) { return false; }
  }
  return true;
}

bool ReadHeights(const GroundQuery &ground,
                 std::span<const double> points,
                 std::vector<double> &heights) {
  heights.clear();
  for (size_t at = 0; at < points.size() / 2; ++at) {
    const auto height = ground.At(PointAt(points, at)).AslM();
    if (!height) { return false; }
    heights.push_back(*height);
  }
  return true;
}

}

bool WaterField::TileGroundResolved(const GroundQuery &ground,
                                    const OsmField &field,
                                    FeatureRun over,
                                    OnLayers on) {
  for (const auto &feature : field.Features().subspan(over.From, over.To - over.From)) {
    const auto kind = KindOf(field, feature, on);
    if (kind == WaterKind::Ignored) { continue; }
    for (const auto &ring : field.Rings().subspan(feature.FirstRing, feature.RingCount)) {
      if (UsableRing(ring, kind) && !RingGroundResolved(ground, RingPoints(field, ring))) {
        return false;
      }
    }
  }
  return true;
}

void WaterField::AddCourse(const OsmField &field,
                           const OsmField::Feature &feature,
                           const OsmField::Ring &ring,
                           const VegetationTemplates &vegetation,
                           std::span<double> heights) {
  if (heights.front() >= heights.back()) {
    for (size_t at = 1; at < heights.size(); ++at) {
      heights[at] = std::min(heights[at], heights[at - 1]);
    }
  } else {
    for (size_t at = heights.size() - 1; at-- > 0;) {
      heights[at] = std::min(heights[at], heights[at + 1]);
    }
  }
  const auto *rule =
      vegetation.Find(field.LayerName(static_cast<int>(feature.Layer)), field.Str(feature, "kind"));
  Course course{};
  course.FirstPoint = ring.First;
  course.PointCount = ring.Count;
  course.FirstLevel = static_cast<uint32_t>(Levels_.size());
  course.HalfWidthM = rule != nullptr && rule->WidthM > 0.0f ? rule->WidthM * 0.5f : 1.0f;
  for (double height : heights) { Levels_.push_back(static_cast<float>(height)); }
  Courses_.push_back(course);
}

void WaterField::AddSurface(const OsmField::Ring &ring, std::span<double> heights) {
  std::ranges::sort(heights);
  const double level =
      heights[static_cast<size_t>(kLevelPercentile * static_cast<double>(heights.size() - 1))];
  if (std::ranges::any_of(heights,
                          [level](double height) { return height > level + kShoreToleranceM; })) {
    ++Outliers_;
  }
  Surfaces_.push_back(
      {.FirstPoint = ring.First, .PointCount = ring.Count, .LevelM = static_cast<float>(level)});
}

uint32_t WaterField::Ingest(const GroundQuery &ground,
                            const OsmField &field,
                            const VegetationTemplates &veg) {
  const auto features = field.Features();
  if (Mark_.Done(features)) { return static_cast<uint32_t>(Surfaces_.size()); }
  const OnLayers layers{.Poly = field.Layer(OsmLayer::WaterPolygons),
                        .Line = field.Layer(OsmLayer::WaterLines)};
  const auto next =
      Mark_.Ask(features,
                field.Tiles(),
                {.CentreX = field.CentreX(), .CentreY = field.CentreY(), .Rings = kEveryRing},
                [&](size_t from, size_t to) {
                  return TileGroundResolved(ground, field, {.From = from, .To = to}, layers);
                });
  if (!next.Found) { return static_cast<uint32_t>(Surfaces_.size()); }
  Mark_.Take(next.Tile);
  Mark_.Advance(features);
  const auto firstSurface = static_cast<uint32_t>(Surfaces_.size());
  std::vector<double> heights;
  for (const auto &feature : features.subspan(next.From, next.To - next.From)) {
    const auto kind = KindOf(field, feature, layers);
    if (kind == WaterKind::Ignored) { continue; }
    for (const auto &ring : field.Rings().subspan(feature.FirstRing, feature.RingCount)) {
      if (!UsableRing(ring, kind)) { continue; }
      if (!ReadHeights(ground, RingPoints(field, ring), heights)) {
        ++NoGround_;
        continue;
      }
      if (kind == WaterKind::Course) {
        AddCourse(field, feature, ring, veg, heights);
      } else {
        AddSurface(ring, heights);
      }
    }
  }
  ByTile_.Set(next.Tile, firstSurface, static_cast<uint32_t>(Surfaces_.size()));
  return static_cast<uint32_t>(Surfaces_.size());
}

}
