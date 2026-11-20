#include "WaterField.h"

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <optional>
#include <ratio>
#include <span>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {

using namespace outshine::Ground;

namespace {

constexpr double kLevelPercentile = 0.05;

constexpr double kShoreToleranceM = 5.0;

constexpr size_t kWaterScansPerAdmission = 4096;
constexpr size_t kPendingHeightsPerTile = 128;
constexpr double kWaterAdmissionBudgetMs = 2.0;

}

namespace {

constexpr uint32_t kMaxWaterRingPoints = 512;
enum class WaterKind { Ignored, Course, Surface };

WaterKind KindOf(const OsmField &field, const OsmField::Feature &feature, OnLayers layers) {
  const bool course = feature.Type == 2 && std::cmp_equal(feature.Layer, layers.Line);
  const bool surface = feature.Type == 3 && std::cmp_equal(feature.Layer, layers.Poly);
  if ((!course && !surface) || field.Num(feature, "tunnel", 0.0) > 0.5) {
    return WaterKind::Ignored;
  }
  return course ? WaterKind::Course : WaterKind::Surface;
}

bool UsableRing(const OsmField::Ring &ring, WaterKind kind) {
  if (ring.Count > kMaxWaterRingPoints) { return false; }
  if (kind == WaterKind::Course) { return ring.Count >= 2; }
  return kind == WaterKind::Surface && ring.Count >= 3;
}

bool IsOcean(const OsmField &field, const OsmField::Feature &feature) {
  return field.Str(feature, "kind") == "ocean";
}

std::span<const double> RingPoints(const OsmField &field, const OsmField::Ring &ring) {
  return field.Points().subspan(static_cast<size_t>(ring.First) * 2,
                                static_cast<size_t>(ring.Count) * 2);
}

LongitudeLatitude PointAt(std::span<const double> points, size_t index) {
  return {.LongitudeDeg = points[index * 2 + 1], .LatitudeDeg = points[index * 2]};
}

bool AdmissionHasBudget(std::chrono::steady_clock::time_point began, size_t steps) {
  return steps < kWaterScansPerAdmission &&
         (steps == 0 ||
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                  .count() < kWaterAdmissionBudgetMs);
}

}

GroundSample WaterField::SampleHeight(const GroundQuery &ground,
                                      LongitudeLatitude point,
                                      Candidate &candidate,
                                      IngestMetrics &metrics) {
  const auto began = std::chrono::steady_clock::now();
  const auto reading = ground.ReadHeight(point);
  metrics.LongestQueryMs = std::max(
      metrics.LongestQueryMs,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  ++metrics.ValidationPoints;
  if (reading.Sample.Where() != GroundSample::State::Pending) {
    ++metrics.AdvancedUnits;
    candidate.Observe(reading);
  }
  return reading.Sample;
}

void WaterField::Candidate::Observe(const GroundQuery::HeightReading &reading) {
  if (!Reusable) { return; }
  if (reading.Sample.Where() != GroundSample::State::Resolved || reading.Certificate == nullptr) {
    Reusable = false;
    return;
  }
  const auto &certificate = *reading.Certificate;
  if (certificate.Dependencies().empty()) { return; }
  if (!certificate.IsComplete()) {
    Reusable = false;
    return;
  }
  const auto found = std::ranges::lower_bound(ObservedBlocks, reading.ResidentRevision);
  if (found != ObservedBlocks.end() && *found == reading.ResidentRevision) { return; }
  ObservedBlocks.insert(found, reading.ResidentRevision);
  if (Terrain.Dependencies().empty()) {
    Terrain = certificate;
  } else {
    Terrain.Merge(certificate);
  }
  Reusable = Terrain.IsComplete();
}

bool WaterField::Candidate::Rebase(const GroundQuery &ground, const OsmField &field) {
  if (!Reusable || (!Terrain.Dependencies().empty() && !ground.HeightInputsCurrent(Terrain))) {
    return false;
  }
  const int tile =
      field.TileIndex(static_cast<int>(Source.Tile.X), static_cast<int>(Source.Tile.Y));
  if (tile < 0) { return false; }
  const auto &current = field.Tiles()[static_cast<size_t>(tile)];
  if (current.Source != Source || current.InputDigest != InputDigest ||
      current.FeatureCount != To - From) {
    return false;
  }
  const size_t firstRing = field.Features()[current.FirstFeature].FirstRing;
  for (auto &sample : Rings) {
    sample.Feature = current.FirstFeature + (sample.Feature - From);
    sample.Ring = firstRing + (sample.Ring - FirstRing);
  }
  Feature = current.FirstFeature + (Feature - From);
  From = current.FirstFeature;
  To = From + current.FeatureCount;
  FirstRing = firstRing;
  Tile = static_cast<uint32_t>(tile);
  return true;
}

bool WaterField::AdvanceCandidate(const GroundQuery &ground,
                                  const OsmField &field,
                                  OnLayers on,
                                  Candidate &candidate,
                                  std::chrono::steady_clock::time_point began,
                                  size_t &steps,
                                  IngestMetrics &metrics) {
  RetryPending(ground, field, candidate, began, steps, metrics);
  while (candidate.Feature < candidate.To && candidate.Pending.size() < kPendingHeightsPerTile &&
         AdmissionHasBudget(began, steps)) {
    ++steps;
    const auto &feature = field.Features()[candidate.Feature];
    const WaterKind kind = KindOf(field, feature, on);
    if (kind == WaterKind::Ignored || candidate.Ring >= feature.RingCount) {
      ++candidate.Feature;
      ++metrics.AdvancedUnits;
      candidate.Ring = 0;
      candidate.Point = 0;
      continue;
    }
    const size_t ringIndex = static_cast<size_t>(feature.FirstRing) + candidate.Ring;
    AdvanceRing(ground, field, kind == WaterKind::Surface, candidate, ringIndex, metrics);
  }
  return candidate.Ready();
}

void WaterField::RetryPending(const GroundQuery &ground,
                              const OsmField &field,
                              Candidate &candidate,
                              std::chrono::steady_clock::time_point began,
                              size_t &steps,
                              IngestMetrics &metrics) {
  auto &pending = candidate.Pending;
  size_t visited = 0;
  size_t waiting = 0;
  for (; visited < pending.size() && AdmissionHasBudget(began, steps); ++visited) {
    ++steps;
    const PendingPoint point = pending[visited];
    auto &samples = candidate.Rings[point.Ring];
    const auto sampled =
        SampleHeight(ground,
                     PointAt(RingPoints(field, field.Rings()[samples.Ring]), point.Point),
                     candidate,
                     metrics);
    if (sampled.Where() == GroundSample::State::Pending) {
      pending[waiting++] = point;
    } else {
      samples.Heights[point.Point] = sampled.AslM();
    }
  }
  const auto unvisited = pending.erase(pending.begin() + static_cast<ptrdiff_t>(waiting),
                                       pending.begin() + static_cast<ptrdiff_t>(visited));
  std::ranges::rotate(pending, unvisited);
}

void WaterField::AdvanceRing(const GroundQuery &ground,
                             const OsmField &field,
                             bool surface,
                             Candidate &candidate,
                             size_t ringIndex,
                             IngestMetrics &metrics) {
  const auto &ring = field.Rings()[ringIndex];
  if (!UsableRing(ring, surface ? WaterKind::Surface : WaterKind::Course)) {
    if (surface) {
      candidate.Rings.push_back(
          {.Feature = candidate.Feature, .Ring = ringIndex, .Heights = {}, .Usable = false});
    }
    ++candidate.Ring;
    ++metrics.AdvancedUnits;
    return;
  }
  if (candidate.Point == 0 &&
      (candidate.Rings.empty() || candidate.Rings.back().Feature != candidate.Feature ||
       candidate.Rings.back().Ring != ringIndex)) {
    candidate.Rings.push_back({.Feature = candidate.Feature, .Ring = ringIndex, .Heights = {}});
    if (!surface || ring.Exterior) { candidate.Rings.back().Heights.reserve(ring.Count); }
  }
  if (surface && (!ring.Exterior || IsOcean(field, field.Features()[candidate.Feature]))) {
    ++candidate.Ring;
    ++metrics.AdvancedUnits;
    return;
  }
  const auto sampled =
      SampleHeight(ground, PointAt(RingPoints(field, ring), candidate.Point), candidate, metrics);
  if (sampled.Where() == GroundSample::State::Pending) {
    candidate.Pending.push_back({.Ring = candidate.Rings.size() - 1, .Point = candidate.Point});
  }
  candidate.Rings.back().Heights.push_back(sampled.AslM());
  ++candidate.Point;
  if (candidate.Point >= ring.Count) {
    ++candidate.Ring;
    candidate.Point = 0;
  }
}

void WaterField::MaterializeCandidate(const OsmField &field,
                                      OnLayers on,
                                      const RegionSurfaceRecipes &vegetation,
                                      const Candidate &candidate) {
  size_t firstSurfaceRing = 0;
  size_t surfaceFeature = 0;
  bool surfaceActive = false;
  const auto publishSurface = [&](size_t end) {
    if (surfaceActive) {
      AddSurface(std::span(candidate.Rings).subspan(firstSurfaceRing, end - firstSurfaceRing),
                 field);
    }
  };
  std::vector<double> heights;
  for (size_t at = 0; at < candidate.Rings.size(); ++at) {
    const RingSamples &samples = candidate.Rings[at];
    const auto &feature = field.Features()[samples.Feature];
    const auto &ring = field.Rings()[samples.Ring];
    if (KindOf(field, feature, on) == WaterKind::Surface) {
      if (!surfaceActive || samples.Feature != surfaceFeature || ring.Exterior) {
        publishSurface(at);
        firstSurfaceRing = at;
        surfaceFeature = samples.Feature;
        surfaceActive = true;
      }
      continue;
    }
    publishSurface(at);
    surfaceActive = false;
    if (!samples.Usable) { continue; }
    if (std::ranges::any_of(samples.Heights,
                            [](const std::optional<double> &height) { return !height; })) {
      ++NoGround_;
      continue;
    }
    heights.clear();
    heights.reserve(samples.Heights.size());
    for (const auto &height : samples.Heights) { heights.push_back(height.value_or(0.0)); }
    AddCourse(field, samples.Feature, ring, vegetation, heights);
  }
  publishSurface(candidate.Rings.size());
}

void WaterField::AddCourse(const OsmField &field,
                           size_t featureAt,
                           const OsmField::Ring &ring,
                           const RegionSurfaceRecipes &vegetation,
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
  const auto &feature = field.Features()[featureAt];
  const auto *rule =
      vegetation.Find(field.LayerName(static_cast<int>(feature.Layer)), field.Str(feature, "kind"));
  Course course{};
  course.FirstPoint = ring.First;
  course.PointCount = ring.Count;
  course.FirstLevel = static_cast<uint32_t>(Levels_.size());
  course.HalfWidthM = rule != nullptr && rule->WidthM > 0.0f ? rule->WidthM * 0.5f : 1.0f;
  for (double height : heights) { Levels_.push_back(static_cast<float>(height)); }
  Courses_.push_back(course);
  CourseFeatures_.push_back(featureAt);
}

void WaterField::AddSurface(std::span<const RingSamples> rings, const OsmField &field) {
  if (rings.empty() || !rings.front().Usable || !field.Rings()[rings.front().Ring].Exterior ||
      SurfaceRings_.size() > std::numeric_limits<uint32_t>::max() - rings.size()) {
    ++InvalidBodies_;
    return;
  }
  for (size_t at = 1; at < rings.size(); ++at) {
    if (!rings[at].Usable || rings[at].Feature != rings.front().Feature ||
        field.Rings()[rings[at].Ring].Exterior) {
      ++InvalidBodies_;
      return;
    }
  }
  if (std::ranges::any_of(rings.front().Heights,
                          [](const std::optional<double> &height) { return !height; })) {
    ++NoGround_;
    return;
  }
  std::vector<double> heights;
  heights.reserve(rings.front().Heights.size());
  for (const auto &height : rings.front().Heights) { heights.push_back(height.value_or(0.0)); }
  const std::optional<float> sampled = IsOcean(field, field.Features()[rings.front().Feature])
                                           ? std::optional<float>{0.0f}
                                           : SurfaceLevel(heights);
  if (!sampled) { return; }
  const double level = *sampled;
  if (std::ranges::any_of(heights,
                          [level](double height) { return height > level + kShoreToleranceM; })) {
    ++Outliers_;
  }
  const auto first = static_cast<uint32_t>(SurfaceRings_.size());
  for (const RingSamples &samples : rings) {
    const OsmField::Ring &ring = field.Rings()[samples.Ring];
    SurfaceRings_.push_back({.FirstPoint = ring.First, .PointCount = ring.Count});
  }
  Surfaces_.push_back({.FirstRing = first,
                       .RingCount = static_cast<uint32_t>(rings.size()),
                       .LevelM = static_cast<float>(level)});
  SurfaceInputs_.push_back({.Feature = rings.front().Feature, .Heights = std::move(heights)});
  SurfaceFeatures_.push_back(rings.front().Feature);
}

void WaterField::RecountSurfaceOutliers() {
  Outliers_ = 0;
  for (size_t at = 0; at < SurfaceInputs_.size(); ++at) {
    const double level = Surfaces_[at].LevelM;
    if (std::ranges::any_of(SurfaceInputs_[at].Heights,
                            [level](double height) { return height > level + kShoreToleranceM; })) {
      ++Outliers_;
    }
  }
}

std::optional<float> WaterField::SurfaceLevel(std::span<double> heights) {
  if (heights.empty()) { return std::nullopt; }
  std::ranges::sort(heights);
  return static_cast<float>(
      heights[static_cast<size_t>(kLevelPercentile * static_cast<double>(heights.size() - 1))]);
}

bool WaterField::MatchesSource(const OsmField &field) const {
  return !SourceGeneration_ ||
         (*SourceGeneration_ == field.Generation() && SourceOrigin_.get() == field.OriginToken());
}

size_t WaterField::SynchronizeSource(const GroundQuery &ground,
                                     const OsmField &field,
                                     const RegionSurfaceRecipes &recipes,
                                     OnLayers layers) {
  const uint64_t terrainScope = ground.TerrainScopeRevision();
  if (MatchesSource(field) && Ground_ == &ground && TerrainScope_ == terrainScope) { return 0; }
  const bool sameInputs = SourceOrigin_.get() == field.OriginToken() && Ground_ == &ground &&
                          terrainScope != 0 && TerrainScope_ == terrainScope;
  auto kept = sameInputs ? std::move(Candidates_) : std::vector<Candidate>{};
  *this = WaterField{};
  SourceGeneration_ = field.Generation();
  SourceOrigin_ = field.ShareOriginToken();
  Ground_ = &ground;
  TerrainScope_ = terrainScope;
  for (auto &previous : kept) {
    if (!previous.Rebase(ground, field)) { continue; }
    if (previous.Ready()) { Admit(field, layers, recipes, previous); }
    Candidates_.push_back(std::move(previous));
  }
  if (Admission_.Takes() != 0 && Admission_.Done(field.Tiles())) { ResolveSurfaceLevels(field); }
  return Admission_.Takes();
}

WaterField::Candidate &WaterField::FindCandidate(const OsmField &field, size_t from, size_t to) {
  const auto &feature = field.Features()[from];
  const uint32_t tile = feature.Tile;
  const auto found = std::ranges::find(Candidates_, tile, &Candidate::Tile);
  if (found != Candidates_.end()) { return *found; }
  const auto &source = field.Tiles()[tile];
  return Candidates_.emplace_back(
      Candidate{.Tile = tile,
                .From = from,
                .To = to,
                .FirstRing = feature.FirstRing,
                .Feature = from,
                .Rings = {},
                .Pending = {},
                .Source = source.Source,
                .InputDigest = source.InputDigest,
                .Terrain = {},
                .ObservedBlocks = {},
                .Reusable = TerrainScope_ != 0 && !source.InputDigest.empty()});
}

void WaterField::Admit(const OsmField &field,
                       OnLayers layers,
                       const RegionSurfaceRecipes &recipes,
                       const Candidate &candidate) {
  Asset_.reset();
  const auto first = static_cast<uint32_t>(Surfaces_.size());
  MaterializeCandidate(field, layers, recipes, candidate);
  ByTile_.Set(candidate.Tile, first, static_cast<uint32_t>(Surfaces_.size()));
  Admission_.Take(candidate.Tile);
}

uint32_t WaterField::Ingest(const GroundQuery &ground,
                            const OsmField &field,
                            const RegionSurfaceRecipes &veg) {
  const auto began = std::chrono::steady_clock::now();
  IngestMetrics metrics;
  const OnLayers layers{.Poly = field.Layer(OsmLayer::WaterPolygons),
                        .Line = field.Layer(OsmLayer::WaterLines)};
  metrics.AdvancedUnits = SynchronizeSource(ground, field, veg, layers);
  ReusedTiles_ += metrics.AdvancedUnits;
  const auto elapsedMs = [](std::chrono::steady_clock::time_point from) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from)
        .count();
  };
  const auto finish = [&] {
    metrics.TotalMs = elapsedMs(began);
    LastIngest_ = metrics;
    if (metrics.TotalMs > WorstIngest_.TotalMs) { WorstIngest_ = metrics; }
    return static_cast<uint32_t>(Surfaces_.size());
  };
  if (Admission_.Done(field.Tiles())) { return finish(); }
  const auto admissionAt = std::chrono::steady_clock::now();
  size_t steps = 0;
  (void)Admission_.Visit(
      field.Tiles(),
      {.CentreX = field.CentreX(), .CentreY = field.CentreY(), .Rings = kEveryRing},
      [&](const TileAdmission::Next &next) {
        if (!AdmissionHasBudget(admissionAt, steps)) { return TileAdmission::Decision::Stop; }
        const auto validationAt = std::chrono::steady_clock::now();
        auto &candidate = FindCandidate(field, next.From, next.To);
        const bool resolved =
            AdvanceCandidate(ground, field, layers, candidate, admissionAt, steps, metrics);
        metrics.ValidationMs += elapsedMs(validationAt);
        if (!resolved) { return TileAdmission::Decision::Defer; }
        const auto materializationAt = std::chrono::steady_clock::now();
        Admit(field, layers, veg, candidate);
        if (!candidate.Reusable) {
          Candidates_.erase(std::ranges::find(Candidates_, next.Tile, &Candidate::Tile));
        }
        metrics.MaterializationMs += elapsedMs(materializationAt);
        return TileAdmission::Decision::Continue;
      });
  metrics.AdmissionMs = elapsedMs(admissionAt) - metrics.MaterializationMs;
  if (Admission_.Done(field.Tiles())) {
    const auto levelsAt = std::chrono::steady_clock::now();
    ResolveSurfaceLevels(field);
    metrics.MaterializationMs += elapsedMs(levelsAt);
  }
  return finish();
}

}
