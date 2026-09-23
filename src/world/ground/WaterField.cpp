#include "WaterField.h"

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <optional>
#include <ratio>
#include <span>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Ground {

namespace {

constexpr double kLevelPercentile = 0.05;

constexpr double kShoreToleranceM = 5.0;

constexpr size_t kWaterCandidatesPerAdmission = 4;
constexpr size_t kWaterAdmissionSteps = 128;
constexpr double kWaterAdmissionBudgetMs = 2.0;

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

}

bool WaterField::AdvanceCandidate(const GroundQuery &ground,
                                  const OsmField &field,
                                  OnLayers on,
                                  Candidate &candidate,
                                  std::chrono::steady_clock::time_point began,
                                  size_t &steps,
                                  IngestMetrics &metrics) {
  while (candidate.Feature < candidate.To) {
    if (steps >= kWaterAdmissionSteps ||
        (steps > 0 &&
         std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                 .count() >= kWaterAdmissionBudgetMs)) {
      return false;
    }
    ++steps;
    const auto &feature = field.Features()[candidate.Feature];
    const WaterKind kind = KindOf(field, feature, on);
    if (kind == WaterKind::Ignored || candidate.Ring >= feature.RingCount) {
      ++candidate.Feature;
      candidate.Ring = 0;
      candidate.Point = 0;
      continue;
    }
    const size_t ringIndex = static_cast<size_t>(feature.FirstRing) + candidate.Ring;
    const auto &ring = field.Rings()[ringIndex];
    if (!UsableRing(ring, kind)) {
      ++candidate.Ring;
      continue;
    }
    if (candidate.Point == 0 &&
        (candidate.Rings.empty() || candidate.Rings.back().Feature != candidate.Feature ||
         candidate.Rings.back().Ring != ringIndex)) {
      candidate.Rings.push_back({.Feature = candidate.Feature, .Ring = ringIndex});
      candidate.Rings.back().Heights.reserve(ring.Count);
    }
    const auto queryAt = std::chrono::steady_clock::now();
    const GroundSample sampled = ground.At(PointAt(RingPoints(field, ring), candidate.Point));
    metrics.LongestQueryMs = std::max(
        metrics.LongestQueryMs,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - queryAt)
            .count());
    ++metrics.ValidationPoints;
    if (sampled.Where() == GroundSample::State::Pending) { return false; }
    candidate.Rings.back().Heights.push_back(sampled.AslM());
    ++candidate.Point;
    if (candidate.Point >= ring.Count) {
      ++candidate.Ring;
      candidate.Point = 0;
    }
  }
  return true;
}

void WaterField::MaterializeCandidate(const OsmField &field,
                                      OnLayers on,
                                      const VegetationTemplates &vegetation,
                                      const Candidate &candidate) {
  std::vector<double> heights;
  for (const RingSamples &samples : candidate.Rings) {
    if (std::ranges::any_of(samples.Heights,
                            [](const std::optional<double> &height) { return !height; })) {
      ++NoGround_;
      continue;
    }
    heights.clear();
    heights.reserve(samples.Heights.size());
    for (const auto &height : samples.Heights) { heights.push_back(*height); }
    const auto &feature = field.Features()[samples.Feature];
    const auto &ring = field.Rings()[samples.Ring];
    if (KindOf(field, feature, on) == WaterKind::Course) {
      AddCourse(field, feature, ring, vegetation, heights);
    } else {
      AddSurface(ring, heights);
    }
  }
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
  const std::optional<float> sampled = SurfaceLevel(heights);
  if (!sampled) { return; }
  const double level = *sampled;
  if (std::ranges::any_of(heights,
                          [level](double height) { return height > level + kShoreToleranceM; })) {
    ++Outliers_;
  }
  Surfaces_.push_back(
      {.FirstPoint = ring.First, .PointCount = ring.Count, .LevelM = static_cast<float>(level)});
}

std::optional<float> WaterField::SurfaceLevel(std::span<double> heights) {
  if (heights.empty()) { return std::nullopt; }
  std::ranges::sort(heights);
  return static_cast<float>(
      heights[static_cast<size_t>(kLevelPercentile * static_cast<double>(heights.size() - 1))]);
}

uint32_t WaterField::Ingest(const GroundQuery &ground,
                            const OsmField &field,
                            const VegetationTemplates &veg) {
  if (SourceGeneration_ != field.Generation()) {
    *this = WaterField{};
    SourceGeneration_ = field.Generation();
  }
  const auto features = field.Features();
  if (Mark_.Done(features)) { return static_cast<uint32_t>(Surfaces_.size()); }
  const auto began = std::chrono::steady_clock::now();
  IngestMetrics metrics;
  const auto elapsedMs = [](std::chrono::steady_clock::time_point from) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from)
        .count();
  };
  const auto finish = [&]() {
    metrics.TotalMs = elapsedMs(began);
    if (metrics.TotalMs > WorstIngest_.TotalMs) { WorstIngest_ = metrics; }
    return static_cast<uint32_t>(Surfaces_.size());
  };
  const OnLayers layers{.Poly = field.Layer(OsmLayer::WaterPolygons),
                        .Line = field.Layer(OsmLayer::WaterLines)};
  const auto admissionAt = std::chrono::steady_clock::now();
  ++Admission_;
  size_t steps = 0;
  const auto next = Mark_.Ask(
      features,
      field.Tiles(),
      {.CentreX = field.CentreX(),
       .CentreY = field.CentreY(),
       .Rings = kEveryRing,
       .CandidatesMost = kWaterCandidatesPerAdmission},
      [&](size_t from, size_t to) {
        const auto validationAt = std::chrono::steady_clock::now();
        const uint32_t tile = features[from].Tile;
        auto found = std::ranges::find_if(
            Candidates_, [tile](const Candidate &one) { return one.Tile == tile; });
        if (found == Candidates_.end()) {
          Candidates_.push_back(
              {.Tile = tile, .LastSeen = Admission_, .From = from, .To = to, .Feature = from});
          found = std::prev(Candidates_.end());
        }
        if (found->From != from || found->To != to) {
          *found = {.Tile = tile, .LastSeen = Admission_, .From = from, .To = to, .Feature = from};
        }
        found->LastSeen = Admission_;
        const bool resolved =
            AdvanceCandidate(ground, field, layers, *found, admissionAt, steps, metrics);
        metrics.ValidationMs += elapsedMs(validationAt);
        return resolved;
      });
  metrics.AdmissionMs = elapsedMs(admissionAt);
  std::erase_if(Candidates_, [this](const Candidate &one) { return one.LastSeen != Admission_; });
  if (!next.Found) { return finish(); }
  const auto firstSurface = static_cast<uint32_t>(Surfaces_.size());
  const auto materializationAt = std::chrono::steady_clock::now();
  const auto staged = std::ranges::find_if(
      Candidates_, [tile = next.Tile](const Candidate &one) { return one.Tile == tile; });
  MaterializeCandidate(field, layers, veg, *staged);
  metrics.MaterializationMs = elapsedMs(materializationAt);
  ByTile_.Set(next.Tile, firstSurface, static_cast<uint32_t>(Surfaces_.size()));
  Mark_.Take(next.Tile);
  Mark_.Advance(features);
  Candidates_.erase(staged);
  return finish();
}

}
