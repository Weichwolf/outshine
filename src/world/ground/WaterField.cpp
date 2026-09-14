#include "WaterField.h"
#include "math/Vec3.h"

#include "Geodesy.h"
#include "TerrainLoader.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Ground {

constexpr double kLeastRunM2 = 1e-9;

namespace {

constexpr double kLevelPercentile = 0.05;

constexpr double kShoreToleranceM = 5.0;

constexpr double kLiftM = 0.15;

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

void WaterField::AnchorAt(const Vec3 &ecef) {
  assert(Surfaces_.empty() && Courses_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
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
  assert(Anchored_);
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

void WaterField::Tessellate(const OsmField &field, std::vector<float> &out) const {
  out.clear();
  const std::span<const double> ring = field.Points();
  std::vector<double> p3;

  for (const Surface &s : Surfaces_) {
    const uint32_t n = s.PointCount;
    if (n < 3) { continue; }
    const double refLat = ring[static_cast<size_t>(s.FirstPoint) * 2];
    const double refLon = ring[static_cast<size_t>(s.FirstPoint) * 2 + 1];
    const Vec3 up = EnuAxesEcef({.LongitudeDeg = refLon, .LatitudeDeg = refLat}).Up;

    p3.resize(static_cast<size_t>(n) * 3);
    for (uint32_t k = 0; k < n; k++) {
      Vec3 p;
      GeoToEcef({.LongitudeDeg = ring[(static_cast<size_t>(s.FirstPoint) + k) * 2 + 1],
                 .LatitudeDeg = ring[(static_cast<size_t>(s.FirstPoint) + k) * 2],
                 .HeightM = s.LevelM + kLiftM},
                p);
      for (int c = 0; c < 3; c++) { p3[static_cast<size_t>(k) * 3 + c] = p[c] - Anchor_[c]; }
    }

    std::vector<double> en(static_cast<size_t>(n) * 2);
    for (uint32_t k = 0; k < n; k++) {
      const EastNorth at =
          EnuOffsetM({.LongitudeDeg = refLon, .LatitudeDeg = refLat},
                     {.LongitudeDeg = ring[(static_cast<size_t>(s.FirstPoint) + k) * 2 + 1],
                      .LatitudeDeg = ring[(static_cast<size_t>(s.FirstPoint) + k) * 2]});
      en[static_cast<size_t>(k) * 2] = at.EastM;
      en[static_cast<size_t>(k) * 2 + 1] = at.NorthM;
    }
    double area2 = 0.0;
    for (uint32_t k = 0; k < n; k++) {
      const uint32_t j = (k + 1) % n;
      area2 += en[static_cast<size_t>(k) * 2] * en[static_cast<size_t>(j) * 2 + 1] -
               en[static_cast<size_t>(j) * 2] * en[static_cast<size_t>(k) * 2 + 1];
    }
    std::vector<uint32_t> poly(n);
    for (uint32_t k = 0; k < n; k++) { poly[k] = area2 >= 0.0 ? k : n - 1 - k; }
    auto cross = [&](uint32_t a2, uint32_t b2, uint32_t c2) {
      return (en[static_cast<size_t>(b2) * 2] - en[static_cast<size_t>(a2) * 2]) *
                 (en[static_cast<size_t>(c2) * 2 + 1] - en[static_cast<size_t>(a2) * 2 + 1]) -
             (en[static_cast<size_t>(b2) * 2 + 1] - en[static_cast<size_t>(a2) * 2 + 1]) *
                 (en[static_cast<size_t>(c2) * 2] - en[static_cast<size_t>(a2) * 2]);
    };
    const auto inside = [&](uint32_t a2, uint32_t b2, uint32_t c2, uint32_t q) {
      return cross(a2, b2, q) >= 0.0 && cross(b2, c2, q) >= 0.0 && cross(c2, a2, q) >= 0.0;
    };
    size_t guard = static_cast<size_t>(n) * static_cast<size_t>(n) + 16;
    while (poly.size() >= 3 && guard-- > 0) {
      bool clipped = false;
      for (size_t k = 0; k < poly.size(); k++) {
        const uint32_t a2 = poly[(k + poly.size() - 1) % poly.size()];
        const uint32_t b2 = poly[k];
        const uint32_t c2 = poly[(k + 1) % poly.size()];
        if (cross(a2, b2, c2) <= 0.0) { continue; }
        bool clear = true;
        for (const uint32_t q : poly) {
          if (q != a2 && q != b2 && q != c2 && inside(a2, b2, c2, q)) {
            clear = false;
            break;
          }
        }
        if (!clear) { continue; }
        for (const uint32_t idx : {a2, b2, c2}) {
          const double *v = &p3[static_cast<size_t>(idx) * 3];
          out.push_back(static_cast<float>(v[0]));
          out.push_back(static_cast<float>(v[1]));
          out.push_back(static_cast<float>(v[2]));
          out.push_back(static_cast<float>(up[0]));
          out.push_back(static_cast<float>(up[1]));
          out.push_back(static_cast<float>(up[2]));
        }
        poly.erase(poly.begin() + static_cast<long>(k));
        clipped = true;
        break;
      }
      if (!clipped) { break; }
    }
  }

  for (const Course &c : Courses_) {
    if (c.PointCount < 2) { continue; }
    const double refLat = ring[static_cast<size_t>(c.FirstPoint) * 2];
    const double refLon = ring[static_cast<size_t>(c.FirstPoint) * 2 + 1];
    const auto [ea, no, up] = EnuAxesEcef({.LongitudeDeg = refLon, .LatitudeDeg = refLat});
    std::vector<double> L(static_cast<size_t>(c.PointCount) * 3);
    std::vector<double> R(static_cast<size_t>(c.PointCount) * 3);
    for (uint32_t k = 0; k < c.PointCount; k++) {
      const uint32_t a = k > 0 ? k - 1 : k;
      const uint32_t b = k + 1 < c.PointCount ? k + 1 : k;
      const LongitudeLatitudeHeight from{.LongitudeDeg = refLon, .LatitudeDeg = refLat};
      const EastNorth before =
          EnuOffsetM(from,
                     {.LongitudeDeg = ring[(static_cast<size_t>(c.FirstPoint) + a) * 2 + 1],
                      .LatitudeDeg = ring[(static_cast<size_t>(c.FirstPoint) + a) * 2]});
      const EastNorth after =
          EnuOffsetM(from,
                     {.LongitudeDeg = ring[(static_cast<size_t>(c.FirstPoint) + b) * 2 + 1],
                      .LatitudeDeg = ring[(static_cast<size_t>(c.FirstPoint) + b) * 2]});
      double tx = after.EastM - before.EastM;
      double ty = after.NorthM - before.NorthM;
      const double tl = std::sqrt(tx * tx + ty * ty);
      if (tl < kLeastRunM2) {
        tx = 1.0;
        ty = 0.0;
      } else {
        tx /= tl;
        ty /= tl;
      }
      const double px = -ty * c.HalfWidthM;
      const double py = tx * c.HalfWidthM;
      const double lat = ring[(static_cast<size_t>(c.FirstPoint) + k) * 2];
      const double lon = ring[(static_cast<size_t>(c.FirstPoint) + k) * 2 + 1];
      const double lev = static_cast<double>(Levels_[c.FirstLevel + k]) + kLiftM;
      Vec3 base;
      GeoToEcef({.LongitudeDeg = lon, .LatitudeDeg = lat, .HeightM = lev}, base);
      for (int cc = 0; cc < 3; cc++) {
        L[static_cast<size_t>(k) * 3 + cc] = base[cc] - Anchor_[cc] + ea[cc] * px + no[cc] * py;
        R[static_cast<size_t>(k) * 3 + cc] = base[cc] - Anchor_[cc] - ea[cc] * px - no[cc] * py;
      }
    }
    for (uint32_t k = 0; k + 1 < c.PointCount; k++) {
      const std::array<const double *, 6> q = {&L[static_cast<size_t>(k) * 3],
                                               &R[static_cast<size_t>(k) * 3],
                                               &R[static_cast<size_t>(k + 1) * 3],
                                               &L[static_cast<size_t>(k) * 3],
                                               &R[static_cast<size_t>(k + 1) * 3],
                                               &L[static_cast<size_t>(k + 1) * 3]};
      for (const auto &t : q) {
        out.push_back(static_cast<float>(t[0]));
        out.push_back(static_cast<float>(t[1]));
        out.push_back(static_cast<float>(t[2]));
        out.push_back(static_cast<float>(up[0]));
        out.push_back(static_cast<float>(up[1]));
        out.push_back(static_cast<float>(up[2]));
      }
    }
  }
}

}
