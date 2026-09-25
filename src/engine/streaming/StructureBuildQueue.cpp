#include <expected>
#include <exception>
#include <atomic>
#include "StructureBuildQueue.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <vector>
#include <string_view>
#include <utility>

#include "Digest.h"
#include "Log.h"
#include "Shape.h"
#include "OsmLayer.h"
#include "Geodesy.h"
#include "StructureSourceKey.h"

namespace outshine {

namespace {

constexpr uint32_t kMostRingPoints = 512;
constexpr uint8_t kPolygonFeature = 3;
constexpr size_t kBuildsPerThread = 1;
constexpr double kBytesPerMB = 1024.0 * 1024.0;

[[nodiscard]] bool EyeWithin(LongitudeLatitude from, LongitudeLatitude to) noexcept {
  const Ellipsoid earth{.SemiMajorM = kWgs84A, .Flattening = 1.0 - std::sqrt(1.0 - kWgs84E2)};
  const Geodesic distance =
      GeodesicOn({.LongitudeDeg = from.LongitudeDeg, .LatitudeDeg = from.LatitudeDeg},
                 {.LongitudeDeg = to.LongitudeDeg, .LatitudeDeg = to.LatitudeDeg},
                 earth);
  return distance.Converged && std::isfinite(distance.AlongM) &&
         distance.AlongM <= Generators::kStructureEyeReuseM;
}

[[nodiscard]] bool AcceptedViewCurrent(const Ground::BuildingField &prints,
                                       LongitudeLatitude eye,
                                       const std::function<bool(uint32_t)> &cellReady) {
  const auto inputs = prints.AcceptedInputs();
  const auto tiles = prints.AcceptedTiles();
  for (size_t at = 0; at < inputs.size(); ++at) {
    if (cellReady ? !cellReady(tiles[at]) : !EyeWithin(inputs[at].Bake.Eye, eye)) { return false; }
  }
  return true;
}

std::optional<Data::TileSourceIdentity> VectorSource(const Ground::OsmField &vectors,
                                                     uint32_t tile) {
  if (tile >= vectors.Tiles().size()) { return std::nullopt; }
  return vectors.Tiles()[tile].Source;
}

uint64_t
StreetDigest(const Ground::StreetField &streets, const Ground::OsmField &vectors, uint32_t tile) {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint64_t word) {
    for (unsigned shift = 0; shift < 64u; shift += 8u) {
      digest = DigestFolded(digest, static_cast<uint8_t>(word >> shift));
    }
  };
  const std::span<const double> points = vectors.Points();
  for (const Ground::StreetField::Way &way : streets.OfTile(static_cast<int>(tile))) {
    const size_t first = way.FirstPoint;
    const size_t count = way.PointCount;
    if (count < 2 || first + count > points.size() / 2u) { continue; }
    fold(count);
    fold(std::bit_cast<uint32_t>(way.HalfWidthM));
    for (size_t at = 0; at < 2u * count; ++at) {
      fold(std::bit_cast<uint64_t>(points[2u * first + at]));
    }
  }
  return digest;
}

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

void RawOf(const Ground::OsmField &vectors,
           const Ground::BuildingField &prints,
           const Ground::StreetField &streets,
           const Ground::TileWatermark::Next &next,
           LongitudeLatitude eye,
           std::optional<LevelOfDetail> detail,
           std::optional<uint32_t> cell,
           Generators::RawTile &raw) {
  raw.LatLon.clear();
  raw.Structures.clear();
  raw.Ways.clear();
  raw.AnchorEcef = prints.Anchor();
  raw.Eye = eye;
  raw.RequestedDetail = detail;
  raw.RequestedCell = cell;
  raw.FocalPx = prints.FocalPx();
  raw.TileSpanM = prints.TileSpanM();
  raw.Extent = vectors.Extent();
  raw.ClusterTriangles = Render::kClusterTriangles;
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  const std::span<const Ground::OsmField::Feature> feats = vectors.Features();
  const std::span<const double> points = vectors.Points();
  const Ground::OsmField::Tile &tile = vectors.Tiles()[next.Tile];
  const Ground::GeoBounds tileBounds = Ground::TileBounds(
      {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)});
  for (const Ground::StreetField::Way &way : streets.OfTile(static_cast<int>(next.Tile))) {
    const auto first = static_cast<size_t>(way.FirstPoint);
    const auto count = static_cast<size_t>(way.PointCount);
    if (count < 2 || first + count > points.size() / 2) { continue; }
    const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(),
                      points.begin() + static_cast<long>(first) * 2,
                      points.begin() + static_cast<long>(first + count) * 2);
    raw.Ways.push_back(
        {.LocalFirst = local, .PointCount = way.PointCount, .HalfWidthM = way.HalfWidthM});
  }
  for (size_t at = next.From; at < next.To; ++at) {
    const Ground::OsmField::Feature &f = feats[at];
    if (f.Type != kPolygonFeature || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const double heightM = vectors.Num(f, "height", 0.0);
    const int pitched = PitchedOf(vectors.Str(f, "roof:shape"));
    for (uint32_t r = 0; r < f.RingCount; ++r) {
      const Ground::OsmField::Ring &ring = vectors.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > kMostRingPoints) { continue; }
      const size_t ringFirst = static_cast<size_t>(ring.First) * 2u;
      const size_t ringLength = static_cast<size_t>(ring.Count) * 2u;
      const std::span<const double> ringPoints(points.data() + ringFirst, ringLength);
      const auto assignedCell = Generators::StructureCellOf(tileBounds, ringPoints);
      if (raw.RequestedCell && (!assignedCell || assignedCell->Index != *raw.RequestedCell)) {
        continue;
      }
      const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
      raw.LatLon.insert(raw.LatLon.end(),
                        points.begin() + static_cast<long>(ring.First) * 2,
                        points.begin() + static_cast<long>(ring.First + ring.Count) * 2);
      raw.Structures.push_back({.LocalFirst = local,
                                .PointCount = ring.Count,
                                .SourceFirst = ring.First,
                                .Cell = assignedCell.value_or(Generators::StructureCell{}),
                                .HeightM = heightM,
                                .Pitched = pitched});
    }
  }
}

bool Gathers(Ground::TileSpot spot,
             bool fineField,
             const StructureBuildQueue::HeightSource &heightAt,
             std::vector<Ground::HeightField::Block> &into) {
  if (std::ranges::any_of(into, [spot](const Ground::HeightField::Block &one) {
        return one.At.X == spot.X && one.At.Y == spot.Y;
      })) {
    return true;
  }
  Ground::HeightField::Block block;
  const Data::TileId tile{
      .Zoom = spot.Zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
  bool copied = fineField && heightAt.CopyField && heightAt.CopyField(tile, block);
  if (!copied && !fineField) {
    constexpr int side = 17;
    copied = Ground::HeightField::SamplesField(tile, side, heightAt.Sample, block);
  }
  if (!copied) { return false; }
  into.push_back(std::move(block));
  return true;
}

std::optional<std::vector<Ground::HeightField::Block>>
BlocksUnder(bool fineField,
            int zoom,
            const Ground::OsmField &vectors,
            Ground::FeatureRun over,
            const StructureBuildQueue::HeightSource &heightAt) {
  const std::span<const Ground::OsmField::Feature> feats = vectors.Features();
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  std::vector<Ground::TileSpot> spots;
  for (size_t at = over.From; at < over.To; ++at) {
    const Ground::OsmField::Feature &f = feats[at];
    if (f.Type != kPolygonFeature || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const Ground::TileSpot low =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MinLon, .LatitudeDeg = f.MaxLat}, zoom);
    const Ground::TileSpot high =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MaxLon, .LatitudeDeg = f.MinLat}, zoom);
    for (long y = low.Y; y <= high.Y; ++y) {
      for (long x = low.X; x <= high.X; ++x) {
        const Ground::TileSpot spot{.Zoom = zoom, .X = x, .Y = y};
        if (std::ranges::none_of(spots, [spot](Ground::TileSpot one) {
              return one.X == spot.X && one.Y == spot.Y;
            })) {
          spots.push_back(spot);
        }
      }
    }
  }
  std::vector<Ground::HeightField::Block> blocks;
  blocks.reserve(spots.size());
  bool complete = true;
  for (const Ground::TileSpot spot : spots) {
    if (!Gathers(spot, fineField, heightAt, blocks)) { complete = false; }
  }
  if (!complete) { return std::nullopt; }
  return blocks;
}

struct HeightResolutionStats {
  size_t &Deferred;
  double &DurationMs;
};

bool ResolveHeights(const Ground::OsmField &vectors,
                    Ground::FeatureRun over,
                    int blockZoom,
                    const StructureBuildQueue::HeightSource &heightAt,
                    StructureBuildQueue::HeightRequirement requirement,
                    std::shared_ptr<const Ground::HeightField> &heights,
                    HeightResolutionStats stats) {
  const auto began = std::chrono::steady_clock::now();
  bool fallback = false;
  std::optional<std::vector<Ground::HeightField::Block>> blocks =
      BlocksUnder(true, blockZoom, vectors, over, heightAt);
  if (!blocks && requirement == StructureBuildQueue::HeightRequirement::AllowFallback) {
    fallback = true;
    blocks = BlocksUnder(false, blockZoom, vectors, over, heightAt);
  }
  if (!blocks) {
    ++stats.Deferred;
    stats.DurationMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    return false;
  }
  auto pinned = Ground::HeightField::Of(blockZoom, std::move(*blocks), fallback);
  if (requirement == StructureBuildQueue::HeightRequirement::FineOnly && !pinned->Qualified()) {
    ++stats.Deferred;
    stats.DurationMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    return false;
  }
  heights = std::move(pinned);
  stats.DurationMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return true;
}

bool CurrentCellSource(const Ground::GroundStack &stack,
                       const Ground::OsmField &vectors,
                       const Ground::BuildingField &footprints,
                       const StructureBuildQueue::HeightSource &heightAt,
                       uint32_t tile,
                       uint64_t sourceKey,
                       size_t &deferred,
                       double &resolutionMs) {
  const auto &record = vectors.Tiles()[tile];
  const Ground::FeatureRun over{.From = record.FirstFeature,
                                .To =
                                    static_cast<size_t>(record.FirstFeature) + record.FeatureCount};
  std::shared_ptr<const Ground::HeightField> heights;
  if (!ResolveHeights(vectors,
                      over,
                      stack.FinestZoomOf(Data::DataKind::Elevation),
                      heightAt,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      heights,
                      {.Deferred = deferred, .DurationMs = resolutionMs}) ||
      !heights) {
    return false;
  }
  return StructureSourceKey({.Vector = VectorSource(vectors, tile),
                             .HeightSources = heights->Sources(),
                             .HeightDigest = heights->RasterDigest(),
                             .StreetDigest = StreetDigest(stack.Ways(), vectors, tile),
                             .TileSpanM = footprints.TileSpanM(),
                             .FallbackHeights = heights->Fallback()}) == sourceKey;
}

struct RefinementSelection {
  std::optional<Ground::TileWatermark::Next> Next;
  bool Deferred = false;
};

template <typename GroundStands>
RefinementSelection SelectRefinement(Ground::BuildingField &prints,
                                     const Ground::OsmField &vectors,
                                     const Ground::StreetField &streets,
                                     std::shared_ptr<const Ground::HeightField> &heights,
                                     GroundStands &&groundStands) {
  const std::optional<uint32_t> tile = prints.RefinementTile();
  if (!tile) { return {.Next = std::nullopt, .Deferred = true}; }
  const std::span<const Ground::OsmField::Feature> features = vectors.Features();
  const auto first = std::ranges::lower_bound(
      features, *tile, std::ranges::less{}, &Ground::OsmField::Feature::Tile);
  const auto last = std::ranges::upper_bound(
      features, *tile, std::ranges::less{}, &Ground::OsmField::Feature::Tile);
  const size_t from = static_cast<size_t>(first - features.begin());
  const size_t to = static_cast<size_t>(last - features.begin());
  if (!groundStands({.From = from, .To = to}) || !heights) {
    return {.Next = std::nullopt, .Deferred = true};
  }
  const Ground::BuildingField::AcceptedInput *const accepted = prints.InputOfTile(*tile);
  const std::optional<Data::TileSourceIdentity> vectorSource = VectorSource(vectors, *tile);
  const bool sourceCurrent = accepted != nullptr && accepted->Qualified &&
                             accepted->Vector == vectorSource &&
                             std::ranges::equal(accepted->Sources, heights->Sources()) &&
                             accepted->Bake.HeightRasterDigest == heights->RasterDigest() &&
                             accepted->Bake.StreetDigest == StreetDigest(streets, vectors, *tile) &&
                             accepted->Bake.TileSpanM == prints.TileSpanM();
  const bool current = sourceCurrent;
  prints.AdvanceRefinement();
  if (current) { return {}; }
  return {.Next =
              Ground::TileWatermark::Next{.From = from, .To = to, .Tile = *tile, .Found = true}};
}

}

StructureBuildQueue::~StructureBuildQueue() {
  Clear();
}

std::optional<uint64_t>
StructureBuildQueue::QualifiedSourceKey(const Ground::BuildingField &footprints, uint32_t tile) {
  const auto *accepted = footprints.InputOfTile(tile);
  if (accepted == nullptr || !accepted->Qualified) { return std::nullopt; }
  return StructureSourceKey({.Vector = accepted->Vector,
                             .HeightSources = accepted->Sources,
                             .HeightDigest = accepted->Bake.HeightRasterDigest,
                             .StreetDigest = accepted->Bake.StreetDigest,
                             .TileSpanM = accepted->Bake.TileSpanM,
                             .FallbackHeights = false});
}

bool StructureBuildQueue::CellSourceCurrent(const Ground::GroundStack &stack,
                                            const Ground::BuildingField &footprints,
                                            const HeightSource &heightAt,
                                            uint32_t tile,
                                            uint64_t sourceKey) {
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr || tile >= vectors->Tiles().size() || sourceKey == 0 ||
      QualifiedSourceKey(footprints, tile) != sourceKey) {
    return false;
  }
  size_t deferred = 0;
  double resolutionMs = 0;
  return CurrentCellSource(
      stack, *vectors, footprints, heightAt, tile, sourceKey, deferred, resolutionMs);
}

bool StructureBuildQueue::BakeRevision::Matches(const Ground::OsmField &vectors,
                                                const Ground::BuildingField &footprints,
                                                LongitudeLatitude eye,
                                                HeightSourceRevision heightSource,
                                                HeightRequirement heights,
                                                std::optional<LevelOfDetail> detail,
                                                BuildPurpose purpose) const noexcept {
  return OwnsReservation(vectors, footprints, eye, heightSource) && RequestedDetail == detail &&
         Purpose == purpose &&
         (purpose == BuildPurpose::SourceGeometry || RequestedDetail || EyeWithin(Eye, eye)) &&
         (heights == HeightRequirement::AllowFallback || !FallbackHeights);
}

bool StructureBuildQueue::Complete(const Ground::GroundStack &stack,
                                   const Ground::BuildingField &footprints,
                                   LongitudeLatitude eye,
                                   const std::function<bool(uint32_t)> &cellReady) const {
  const Ground::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr ||
         (Queue_.empty() && footprints.RefinementComplete() &&
          AcceptedViewCurrent(footprints, eye, cellReady) && footprints.Ingested(*vectors));
}

bool StructureBuildQueue::SourcesComplete(const Ground::GroundStack &stack,
                                          const Ground::BuildingField &footprints) const {
  return Queue_.empty() && QualifiedSources(stack, footprints);
}

bool StructureBuildQueue::QualifiedSources(const Ground::GroundStack &stack,
                                           const Ground::BuildingField &footprints) {
  const Ground::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr ||
         (footprints.RefinementComplete() && footprints.Ingested(*vectors) &&
          std::ranges::all_of(footprints.AcceptedInputs(),
                              [](const auto &input) { return input.Qualified; }));
}

size_t StructureBuildQueue::QueuedStructures() const {
  size_t count = 0;
  const auto countRemaining = [&count](const std::deque<QueuedBuild> &queue) {
    for (const QueuedBuild &bake : queue) {
      const size_t all = bake.Task.Raw().Structures.size();
      count += all > bake.BakedStructures ? all - bake.BakedStructures : 0;
    }
  };
  countRemaining(Queue_);
  countRemaining(CellQueue_);
  return count;
}

std::unique_ptr<MeshScratch> StructureBuildQueue::LentScratch() {
  if (IdleScratch_.empty()) { return Mesher_->Scratch(); }
  std::unique_ptr<MeshScratch> one = std::move(IdleScratch_.back());
  IdleScratch_.pop_back();
  return one;
}

void StructureBuildQueue::PostSlice(QueuedBuild &build) {
  if (build.Tasks == 0) {
    build.Task.Start(*Pool_, *Mesher_);
  } else {
    build.Task.Resume(*Pool_, *Mesher_);
  }
  build.Finished = false;
}

void StructureBuildQueue::DiscardStale(const Ground::OsmField &vectors,
                                       Ground::BuildingField &prints,
                                       LongitudeLatitude eye,
                                       HeightSourceRevision heightSource,
                                       HeightRequirement heights,
                                       std::optional<LevelOfDetail> detail,
                                       BuildPurpose purpose) {
  while (!Queue_.empty() && !Queue_.front().Revision.Matches(
                                vectors, prints, eye, heightSource, heights, detail, purpose)) {
    QueuedBuild &stale = Queue_.front();
    if (!stale.Finished) { stale.Finished = stale.Task.TakeCompletion(*Pool_); }
    if (!stale.Finished) { return; }
    IdleRaw_.reserve(IdleRaw_.size() + 1u);
    IdleOut_.reserve(IdleOut_.size() + 1u);
    IdleScratch_.reserve(IdleScratch_.size() + 1u);
    if (!stale.Replacement && stale.Revision.OwnsReservation(vectors, prints, eye, heightSource)) {
      prints.Release(stale.Task.Tile());
    }
    IdleRaw_.push_back(stale.Task.TakeRaw());
    IdleOut_.push_back(stale.Task.TakeOutput());
    IdleScratch_.push_back(stale.Task.TakeScratch());
    Queue_.pop_front();
    ++Discarded_;
  }
}

void StructureBuildQueue::ResumeCompletedTasks() {
  const auto resume = [this](std::deque<QueuedBuild> &queue) {
    for (QueuedBuild &bake : queue) {
      if (!bake.Finished) {
        bake.Finished = bake.Task.TakeCompletion(*Pool_);
        if (bake.Finished) {
          bake.BakedStructures = bake.Task.Progress().BakedStructures();
          ++bake.Tasks;
          CompletedRanges_ += bake.Task.Result().LastRanges;
          SlowestRangeMs_ = std::max(SlowestRangeMs_, bake.Task.Result().LastRangeMs);
          SlowestFinalizationMs_ =
              std::max(SlowestFinalizationMs_, bake.Task.Result().FinalizationMs);
          SlowestQueueMs_ = std::max(SlowestQueueMs_, bake.Task.Result().LastQueueMs);
          SlowestTaskMs_ = std::max(SlowestTaskMs_, bake.Task.Result().LastTaskMs);
        }
      }
      if (bake.Finished && bake.Task.Result().Status && !bake.Task.Result().Tile) {
        PostSlice(bake);
      }
    }
  };
  resume(Queue_);
  resume(CellQueue_);
}

bool StructureBuildQueue::PostsCell(Ground::GroundStack &stack,
                                    Ground::BuildingField &footprints,
                                    LongitudeLatitude eye,
                                    const HeightSource &heightAt,
                                    CellRequest request) {
  const Ground::OsmField *vectors = stack.Vectors();
  if (Pool_ == nullptr || Mesher_ == nullptr || vectors == nullptr || !footprints.Anchored() ||
      request.Tile >= vectors->Tiles().size() || request.Cell == 0 ||
      request.Cell > Generators::kStructureCellsPerTile || request.Detail > LevelOfDetail::Massed ||
      request.SourceKey == 0 ||
      Queue_.size() + CellQueue_.size() >=
          std::min(static_cast<size_t>(Pool_->Threads()) * kBuildsPerThread, kCandidateWindow)) {
    return false;
  }
  const auto *accepted = footprints.InputOfTile(request.Tile);
  const auto sourceKey = QualifiedSourceKey(footprints, request.Tile);
  if (!sourceKey || *sourceKey != request.SourceKey ||
      (accepted->OccupiedCells & (uint64_t{1} << (request.Cell - 1u))) == 0 ||
      accepted->Vector != VectorSource(*vectors, request.Tile) || CellQueued(request)) {
    return false;
  }
  const auto &tile = vectors->Tiles()[request.Tile];
  const Ground::FeatureRun over{.From = tile.FirstFeature,
                                .To = static_cast<size_t>(tile.FirstFeature) + tile.FeatureCount};
  std::shared_ptr<const Ground::HeightField> heights;
  double heightResolutionMs = 0.0;
  if (!ResolveHeights(*vectors,
                      over,
                      stack.FinestZoomOf(Data::DataKind::Elevation),
                      heightAt,
                      HeightRequirement::FineOnly,
                      heights,
                      {.Deferred = Deferred_, .DurationMs = heightResolutionMs}) ||
      !heights) {
    return false;
  }
  SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, heightResolutionMs);
  const uint64_t streetDigest = StreetDigest(stack.Ways(), *vectors, request.Tile);
  const uint64_t pinnedKey = StructureSourceKey({.Vector = VectorSource(*vectors, request.Tile),
                                                 .HeightSources = heights->Sources(),
                                                 .HeightDigest = heights->RasterDigest(),
                                                 .StreetDigest = streetDigest,
                                                 .TileSpanM = footprints.TileSpanM(),
                                                 .FallbackHeights = heights->Fallback()});
  if (pinnedKey != request.SourceKey) { return false; }
  const size_t recycleCapacity = IdleRaw_.size() + Queue_.size() + CellQueue_.size() + 1u;
  IdleRaw_.reserve(recycleCapacity);
  IdleOut_.reserve(recycleCapacity);
  IdleScratch_.reserve(recycleCapacity);
  std::unique_ptr<Generators::RawTile> raw = Borrowed(IdleRaw_);
  const Ground::TileWatermark::Next next{
      .From = over.From, .To = over.To, .Tile = request.Tile, .Found = true};
  const auto extractionAt = std::chrono::steady_clock::now();
  RawOf(*vectors, footprints, stack.Ways(), next, eye, request.Detail, request.Cell, *raw);
  SlowestRawExtractionMs_ = std::max(
      SlowestRawExtractionMs_,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - extractionAt)
          .count());
  std::unique_ptr<StructureBuildTask::Output> output = Borrowed(IdleOut_);
  *output = {};
  CellQueue_.push_back(
      {.Revision = {.Vectors = vectors->Generation(),
                    .HeightSource = heightAt.Revision,
                    .TileSpanM = footprints.TileSpanM(),
                    .Eye = eye,
                    .RequestedDetail = request.Detail,
                    .Purpose = BuildPurpose::ViewDetail},
       .Task = StructureBuildTask(
           request.Tile, std::move(raw), std::move(heights), std::move(output), LentScratch()),
       .StreetDigest = streetDigest,
       .SourceKey = request.SourceKey,
       .Cell = request.Cell});
  PostSlice(CellQueue_.back());
  ++Posted_;
  return true;
}

size_t StructureBuildQueue::Posts(Ground::GroundStack &stack,
                                  Ground::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  const HeightSource &heightAt,
                                  size_t candidatesMost,
                                  HeightRequirement requirement,
                                  std::optional<LevelOfDetail> detail,
                                  BuildPurpose purpose,
                                  const std::function<bool(uint32_t)> &cellReady) {
  if (Pool_ == nullptr || Mesher_ == nullptr || stack.Vectors() == nullptr || !prints.Anchored()) {
    return 0;
  }
  const Ground::OsmField &vectors = *stack.Vectors();
  if (requirement == HeightRequirement::FineOnly && purpose == BuildPurpose::ViewDetail &&
      !cellReady && prints.RefinementComplete() && Queue_.empty() &&
      !AcceptedViewCurrent(prints, eye, cellReady)) {
    prints.BeginRefinement();
  }
  size_t posted = 0;
  const size_t inFlightMost =
      std::min(static_cast<size_t>(Pool_->Threads()) * kBuildsPerThread, kCandidateWindow);
  const int blockZoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  size_t examined = 0;
  while (Queue_.size() + CellQueue_.size() < inFlightMost) {
    std::shared_ptr<const Ground::HeightField> heights;
    double heightResolutionMs = 0.0;
    const auto groundStands = [&](Ground::FeatureRun over) {
      return ResolveHeights(vectors,
                            over,
                            blockZoom,
                            heightAt,
                            requirement,
                            heights,
                            {.Deferred = Deferred_, .DurationMs = heightResolutionMs});
    };
    const auto selectionAt = std::chrono::steady_clock::now();
    std::optional<Ground::TileWatermark::Next> next;
    bool replacement = false;
    if (requirement == HeightRequirement::FineOnly && !prints.RefinementComplete()) {
      if (examined >= candidatesMost) { break; }
      ++examined;
      const RefinementSelection selected =
          SelectRefinement(prints, vectors, stack.Ways(), heights, groundStands);
      if (selected.Deferred) { break; }
      if (!selected.Next) { continue; }
      next = selected.Next;
      replacement = true;
    } else {
      next = prints.Next(vectors, groundStands, candidatesMost);
    }
    SlowestCandidateSelectionMs_ = std::max(
        SlowestCandidateSelectionMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - selectionAt)
            .count());
    SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, heightResolutionMs);
    if (!next || !heights) { break; }
    const BakeRevision revision{.Vectors = vectors.Generation(),
                                .HeightSource = heightAt.Revision,
                                .FocalPx = prints.FocalPx(),
                                .TileSpanM = prints.TileSpanM(),
                                .Eye = eye,
                                .RequestedDetail = detail,
                                .Purpose = purpose,
                                .FallbackHeights = heights->Fallback()};
    if (!replacement) { prints.Take(next->Tile); }
    std::unique_ptr<Generators::RawTile> raw = Borrowed(IdleRaw_);
    const auto extractionAt = std::chrono::steady_clock::now();
    RawOf(vectors, prints, stack.Ways(), *next, eye, detail, std::nullopt, *raw);
    const uint64_t streetDigest = StreetDigest(stack.Ways(), vectors, next->Tile);
    const uint64_t sourceKey = StructureSourceKey({.Vector = VectorSource(vectors, next->Tile),
                                                   .HeightSources = heights->Sources(),
                                                   .HeightDigest = heights->RasterDigest(),
                                                   .StreetDigest = streetDigest,
                                                   .TileSpanM = prints.TileSpanM(),
                                                   .FallbackHeights = heights->Fallback()});
    SlowestRawExtractionMs_ = std::max(
        SlowestRawExtractionMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - extractionAt)
            .count());
    std::unique_ptr<StructureBuildTask::Output> output = Borrowed(IdleOut_);
    output->Status = {};
    output->Tile.reset();
    output->BakeMs = 0.0;
    output->LastRanges = 0;
    output->LastRangeMs = 0.0;
    output->FinalizationMs = 0.0;
    output->LastQueueMs = 0.0;
    output->LastTaskMs = 0.0;
    Queue_.push_back(
        {.Revision = revision,
         .Task = StructureBuildTask(
             next->Tile, std::move(raw), std::move(heights), std::move(output), LentScratch()),
         .StreetDigest = streetDigest,
         .SourceKey = sourceKey,
         .Replacement = replacement});
    const auto postingAt = std::chrono::steady_clock::now();
    PostSlice(Queue_.back());
    SlowestTaskPostingMs_ = std::max(
        SlowestTaskPostingMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postingAt)
            .count());
    ++Posted_;
    ++posted;
  }
  return posted;
}

std::expected<std::vector<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextLandings(Ground::GroundStack &stack,
                                  Ground::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  HeightSourceRevision heightSource,
                                  size_t most,
                                  HeightRequirement heights,
                                  std::optional<LevelOfDetail> detail,
                                  BuildPurpose purpose) {
  std::vector<Landing> landings;
  if (Pool_ == nullptr || most == 0) { return landings; }
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr) { return landings; }
  DiscardStale(*vectors, prints, eye, heightSource, heights, detail, purpose);
  ResumeCompletedTasks();
  size_t count = 0;
  size_t printCount = 0;
  size_t spreadCount = 0;
  size_t acrossCount = 0;
  while (count < most && count < Queue_.size()) {
    QueuedBuild &bake = Queue_[count];
    if (!bake.Finished ||
        !bake.Revision.Matches(*vectors, prints, eye, heightSource, heights, detail, purpose)) {
      break;
    }
    if (!bake.Task.Result().Status) {
      if (count == 0) { return std::unexpected(bake.Task.Result().Status.error()); }
      break;
    }
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { break; }
    const Generators::BakedTile &baked = *completed;
    printCount += baked.Prints.size();
    spreadCount += baked.SeatSpreadM.size();
    acrossCount += baked.AcrossM.size();
    ++count;
  }
  if (count == 0) { return landings; }
  IdleRaw_.reserve(IdleRaw_.size() + count);
  IdleOut_.reserve(IdleOut_.size() + count);
  IdleScratch_.reserve(IdleScratch_.size() + count);
  prints.PreparesAcceptances(
      {.Prints = printCount, .Spread = spreadCount, .Across = acrossCount, .Tiles = count});
  landings.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const QueuedBuild &bake = Queue_[at];
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { std::terminate(); }
    const Generators::BakedTile &baked = *completed;
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    const std::optional<Data::TileSourceIdentity> vectorSource =
        VectorSource(*vectors, bake.Task.Tile());
    landings.push_back({.Tile = bake.Task.Tile(),
                        .Baked = &baked,
                        .AnchorEcef = bake.Task.Raw().AnchorEcef,
                        .SourceKey = bake.SourceKey,
                        .Footprints = prints.PrepareAcceptance(
                            bake.Task.Tile(),
                            {.Prints = baked.Prints,
                             .SeatSpreadM = baked.SeatSpreadM,
                             .AcrossM = baked.AcrossM,
                             .OccupiedCells = baked.OccupiedCells,
                             .CellBounds = baked.CellBounds,
                             .CellMaxHeightM = baked.CellMaxHeightM,
                             .Triangles = triangles,
                             .OsmHeights = baked.OsmHeights,
                             .DefaultHeights = baked.DefaultHeights,
                             .Fronted = baked.Fronted},
                            bake.Task.Heights().Sources(),
                            bake.Task.Heights().Qualified(),
                            vectorSource,
                            {.HeightRasterDigest = bake.Task.Heights().RasterDigest(),
                             .StreetDigest = bake.StreetDigest,
                             .FocalPx = bake.Revision.FocalPx,
                             .TileSpanM = bake.Revision.TileSpanM,
                             .Eye = bake.Revision.Eye})});
  }
  return landings;
}

std::expected<std::optional<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextCellLanding(const Ground::GroundStack &stack,
                                     const Ground::BuildingField &footprints,
                                     const HeightSource &heightAt) {
  if (CellQueue_.empty() || Pool_ == nullptr) { return std::nullopt; }
  QueuedBuild &bake = CellQueue_.front();
  const auto discard = [this, &bake] {
    IdleRaw_.push_back(bake.Task.TakeRaw());
    IdleOut_.push_back(bake.Task.TakeOutput());
    IdleScratch_.push_back(bake.Task.TakeScratch());
    CellQueue_.pop_front();
    ++Discarded_;
  };
  const Ground::OsmField *vectors = stack.Vectors();
  const auto *accepted = footprints.InputOfTile(bake.Task.Tile());
  const bool current = vectors != nullptr && accepted != nullptr &&
                       bake.Revision.Vectors == vectors->Generation() &&
                       bake.Revision.HeightSource == heightAt.Revision &&
                       QualifiedSourceKey(footprints, bake.Task.Tile()) == bake.SourceKey &&
                       (accepted->OccupiedCells & (uint64_t{1} << (bake.Cell - 1u))) != 0 &&
                       bake.Task.Tile() < vectors->Tiles().size() &&
                       VectorSource(*vectors, bake.Task.Tile()) == accepted->Vector;
  if (!current) {
    bake.Task.RequestStop();
    if (!bake.Finished) { bake.Finished = bake.Task.TakeCompletion(*Pool_); }
    if (bake.Finished) { discard(); }
    return std::nullopt;
  }
  ResumeCompletedTasks();
  if (!bake.Finished) { return std::nullopt; }
  double resolutionMs = 0.0;
  if (!CurrentCellSource(stack,
                         *vectors,
                         footprints,
                         heightAt,
                         bake.Task.Tile(),
                         bake.SourceKey,
                         Deferred_,
                         resolutionMs)) {
    discard();
    return std::nullopt;
  }
  SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, resolutionMs);
  if (!bake.Task.Result().Status) { return std::unexpected(bake.Task.Result().Status.error()); }
  const auto &completed = bake.Task.Result().Tile;
  if (!completed) { return std::nullopt; }
  if (completed->RequestedCell != bake.Cell ||
      completed->RequestedDetail != bake.Revision.RequestedDetail ||
      completed->OccupiedCells != (uint64_t{1} << (bake.Cell - 1u))) {
    return std::unexpected(
        Generators::StructureBakeError{Generators::StructureBakeErrorKind::InvalidCell});
  }
  return std::optional<Landing>{{.Tile = bake.Task.Tile(),
                                 .Baked = &*completed,
                                 .AnchorEcef = bake.Task.Raw().AnchorEcef,
                                 .SourceKey = bake.SourceKey,
                                 .Footprints = std::nullopt}};
}

void StructureBuildQueue::CommitsCellLanding(const Landing &landing) noexcept {
  assert(!CellQueue_.empty());
  QueuedBuild &bake = CellQueue_.front();
  const auto &tile = bake.Task.Result().Tile;
  const bool sameBaked =
      tile.transform([&](const auto &baked) { return landing.Baked == &baked; }).value_or(false);
  if (!sameBaked) { std::terminate(); }
  assert(bake.Finished && landing.Tile == bake.Task.Tile() && landing.SourceKey == bake.SourceKey &&
         !landing.Footprints);
  BakedMs_ += bake.Task.Result().BakeMs;
  SlowestBakeMs_ = std::max(SlowestBakeMs_, bake.Task.Result().BakeMs);
  IdleRaw_.push_back(bake.Task.TakeRaw());
  IdleOut_.push_back(bake.Task.TakeOutput());
  IdleScratch_.push_back(bake.Task.TakeScratch());
  CellQueue_.pop_front();
  ++Landed_;
}

bool StructureBuildQueue::CellQueued(CellRequest request) const noexcept {
  return std::ranges::any_of(CellQueue_, [request](const QueuedBuild &queued) {
    return queued.Task.Tile() == request.Tile && queued.Cell == request.Cell &&
           queued.Revision.RequestedDetail == request.Detail &&
           queued.SourceKey == request.SourceKey;
  });
}

void StructureBuildQueue::CommitsLandings(Ground::GroundStack &stack,
                                          Ground::BuildingField &footprints,
                                          std::span<Landing> landings) noexcept {
  for (Landing &landing : landings) {
    QueuedBuild &bake = Queue_.front();
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { std::terminate(); }
    const Generators::BakedTile &baked = *completed;
    assert(landing.Tile == bake.Task.Tile() && landing.Baked == &baked && landing.Footprints);
    if (!landing.Footprints) { std::terminate(); }
    BakedMs_ += bake.Task.Result().BakeMs;
    SlowestBakeMs_ = std::max(SlowestBakeMs_, bake.Task.Result().BakeMs);
    assert(IdleRaw_.size() < IdleRaw_.capacity() && IdleOut_.size() < IdleOut_.capacity() &&
           IdleScratch_.size() < IdleScratch_.capacity());
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    const Ground::BuildingField::Baked product{.Prints = baked.Prints,
                                               .SeatSpreadM = baked.SeatSpreadM,
                                               .AcrossM = baked.AcrossM,
                                               .OccupiedCells = baked.OccupiedCells,
                                               .CellBounds = baked.CellBounds,
                                               .CellMaxHeightM = baked.CellMaxHeightM,
                                               .Triangles = triangles,
                                               .OsmHeights = baked.OsmHeights,
                                               .DefaultHeights = baked.DefaultHeights,
                                               .Fronted = baked.Fronted};
    if (bake.Replacement) {
      footprints.ReplaceAcceptance(std::move(landing.Footprints.value()), product);
    } else {
      footprints.CommitAcceptance(std::move(landing.Footprints.value()), *stack.Vectors(), product);
    }
    Log::Info(LogTag::World,
              "buildings",
              {{"added", static_cast<int>(baked.Prints.size())},
               {"total", static_cast<int>(footprints.Footprints().size())},
               {"osmHeight", footprints.OsmHeights()},
               {"defaultHeight", footprints.DefaultHeights()},
               {"vertsMB", static_cast<double>(baked.Built.UsedBytes()) / kBytesPerMB},
               {"lumped", baked.Lumped},
               {"blocks", baked.Blocks},
               {"unsupportedMeshes", static_cast<double>(baked.UnsupportedMeshes)},
               {"bakeMs", bake.Task.Result().BakeMs},
               {"queued", static_cast<int>(Queue_.size() - 1)}});
    IdleRaw_.push_back(bake.Task.TakeRaw());
    IdleOut_.push_back(bake.Task.TakeOutput());
    IdleScratch_.push_back(bake.Task.TakeScratch());
    Queue_.pop_front();
    ++Landed_;
  }
}

void StructureBuildQueue::Clear() {
  for (QueuedBuild &bake : Queue_) {
    bake.Task.RequestStop();
    if (Pool_ != nullptr) { bake.Task.Join(*Pool_); }
  }
  for (QueuedBuild &bake : CellQueue_) {
    bake.Task.RequestStop();
    if (Pool_ != nullptr) { bake.Task.Join(*Pool_); }
  }
  Queue_.clear();
  CellQueue_.clear();
  IdleRaw_.clear();
  IdleOut_.clear();
  IdleScratch_.clear();
}

}
