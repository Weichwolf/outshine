#include <expected>
#include <exception>
#include <atomic>
#include "StructureBuildQueue.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <limits>
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
constexpr size_t kPinnedCellHeightBytesMost = size_t{2} * 1024u * 1024u;
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

std::optional<uint64_t>
StreetDigest(const Ground::StreetField &streets, const Ground::OsmField &vectors, uint32_t tile) {
  return streets.SourceDigest(vectors, tile);
}

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

void AppendInnerRings(std::span<const GeographicRing> rings,
                      std::span<const double> points,
                      Generators::RawTile &raw) {
  for (const auto &hole : rings) {
    if (hole.Exterior) { break; }
    const auto holeFirst = static_cast<uint32_t>(raw.LatLon.size() / 2);
    const auto contour =
        points.subspan(static_cast<size_t>(hole.First) * 2, static_cast<size_t>(hole.Count) * 2);
    raw.LatLon.insert(raw.LatLon.end(), contour.begin(), contour.end());
    raw.Holes.push_back({.First = holeFirst, .Count = hole.Count, .Exterior = false});
  }
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
  raw.Holes.clear();
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
      const auto firstHole = static_cast<uint32_t>(raw.Holes.size());
      AppendInnerRings(
          vectors.Rings().subspan(f.FirstRing + r + 1, f.RingCount - r - 1), points, raw);
      raw.Structures.push_back({.LocalFirst = local,
                                .PointCount = ring.Count,
                                .SourceFirst = ring.First,
                                .FirstHole = firstHole,
                                .HoleCount = static_cast<uint32_t>(raw.Holes.size()) - firstHole,
                                .SourceFirstHole = f.FirstRing + r + 1,
                                .Cell = assignedCell.value_or(Generators::StructureCell{}),
                                .HeightM = heightM,
                                .MinimumHeightM = vectors.Num(f, "min_height", 0.0),
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

bool QualifiedStructureHeights(const Ground::OsmField &vectors,
                               Ground::FeatureRun over,
                               const Ground::HeightField &heights) {
  if (heights.Qualified()) { return true; }
  if (heights.Fallback() || !heights.Blocks().empty()) { return false; }
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  return std::ranges::none_of(
      vectors.Features().subspan(over.From, over.To - over.From), [layer](const auto &feature) {
        return feature.Type == kPolygonFeature && std::cmp_equal(feature.Layer, layer);
      });
}

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
  if (!pinned->Certificate().ScopeCurrent(heightAt.TerrainScope)) {
    ++stats.Deferred;
    stats.DurationMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    return false;
  }
  if (requirement == StructureBuildQueue::HeightRequirement::FineOnly &&
      !QualifiedStructureHeights(vectors, over, *pinned)) {
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

StructureBuildQueue::CellSourceState
InspectAcceptedSourceMetadata(const Ground::OsmField &vectors,
                              const Ground::StreetField &streets,
                              const Ground::BuildingField &footprints,
                              uint32_t tile,
                              uint64_t sourceKey) {
  using State = StructureBuildQueue::CellSourceState;
  if (tile >= vectors.Tiles().size()) { return State::Unknown; }
  const auto *accepted = footprints.InputOfTile(tile);
  if (accepted == nullptr || !accepted->Qualified) { return State::Unknown; }
  if (sourceKey == 0 || accepted->Vector != vectors.Tiles()[tile].Source ||
      accepted->Bake.TileSpanM != footprints.TileSpanM() ||
      StructureBuildQueue::QualifiedSourceKey(footprints, tile) != sourceKey) {
    return State::Stale;
  }
  const auto street = StreetDigest(streets, vectors, tile);
  if (!street) { return State::Unknown; }
  return accepted->Bake.StreetDigest == *street ? State::Current : State::Stale;
}

bool CertifiedAcceptedSourceCurrent(const Ground::OsmField &vectors,
                                    const Ground::StreetField &streets,
                                    const Ground::BuildingField &footprints,
                                    const StructureBuildQueue::HeightSource &heightAt,
                                    uint32_t tile,
                                    uint64_t sourceKey) {
  const auto *accepted = footprints.InputOfTile(tile);
  return InspectAcceptedSourceMetadata(vectors, streets, footprints, tile, sourceKey) ==
             StructureBuildQueue::CellSourceState::Current &&
         accepted->Terrain.IsComplete() && accepted->Terrain.ScopeCurrent(heightAt.TerrainScope) &&
         heightAt.CertificateCurrent && heightAt.CertificateCurrent(accepted->Terrain);
}

bool ValidateCellSource(const Ground::GroundStack &stack,
                        const Ground::OsmField &vectors,
                        const Ground::BuildingField &footprints,
                        const StructureBuildQueue::HeightSource &heightAt,
                        uint32_t tile,
                        uint64_t sourceKey,
                        size_t &deferred,
                        double &resolutionMs) {
  if (CertifiedAcceptedSourceCurrent(
          vectors, stack.Ways(), footprints, heightAt, tile, sourceKey)) {
    return true;
  }
  const auto street = StreetDigest(stack.Ways(), vectors, tile);
  if (!street) {
    ++deferred;
    return false;
  }
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
      !heights || !heights->Certificate().ScopeCurrent(heightAt.TerrainScope)) {
    return false;
  }
  return StructureSourceKey({.Vector = VectorSource(vectors, tile),
                             .HeightSources = heights->Sources(),
                             .HeightDigest = heights->RasterDigest(),
                             .StreetDigest = *street,
                             .TileSpanM = footprints.TileSpanM(),
                             .FallbackHeights = heights->Fallback()}) == sourceKey;
}

bool InstrumentedHeightsCurrent(const Ground::HeightField &heights,
                                const StructureBuildQueue::HeightSource &source) {
  const auto &certificate = heights.Certificate();
  return certificate.ScopeCurrent(source.TerrainScope) &&
         (certificate.Dependencies().empty() ||
          (certificate.IsComplete() && source.CertificateCurrent &&
           source.CertificateCurrent(certificate)));
}

bool PinnedHeightsResident(const Ground::HeightField &pinned,
                           const StructureBuildQueue::HeightSource &heightAt) {
  if (!pinned.Certificate().ScopeCurrent(heightAt.TerrainScope)) { return false; }
  if (!pinned.Certificate().Dependencies().empty()) {
    return pinned.Certificate().IsComplete() && heightAt.CertificateCurrent &&
           heightAt.CertificateCurrent(pinned.Certificate());
  }
  if (!heightAt.ResidentField || pinned.Blocks().empty() || pinned.Fallback()) { return false; }
  return std::ranges::all_of(pinned.Blocks(), [&](const Ground::HeightField::Block &block) {
    if (!block.Terrain || block.At.Zoom < 0 || !std::in_range<uint32_t>(block.At.X) ||
        !std::in_range<uint32_t>(block.At.Y)) {
      return false;
    }
    const Data::TileId at{.Zoom = block.At.Zoom,
                          .X = static_cast<uint32_t>(block.At.X),
                          .Y = static_cast<uint32_t>(block.At.Y)};
    return heightAt.ResidentField(at) == block.Terrain;
  });
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
                                     GroundStands &&groundStands,
                                     size_t &remaining) {
  if (remaining == 0) { return {.Next = std::nullopt, .Deferred = true}; }
  --remaining;
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
  if (sourceCurrent) {
    prints.AdvanceRefinement();
    return {};
  }
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
  return accepted->SourceKey;
}

StructureBuildQueue::CellSourceState
StructureBuildQueue::InspectCellSource(const Ground::GroundStack &stack,
                                       const Ground::BuildingField &footprints,
                                       const HeightSource &heightAt,
                                       uint32_t tile,
                                       uint64_t sourceKey) {
  const auto *vectors = stack.Vectors();
  const auto *accepted = footprints.InputOfTile(tile);
  if (vectors == nullptr || tile >= vectors->Tiles().size() || accepted == nullptr ||
      !accepted->Qualified) {
    return CellSourceState::Unknown;
  }
  const auto metadata =
      InspectAcceptedSourceMetadata(*vectors, stack.Ways(), footprints, tile, sourceKey);
  if (metadata == CellSourceState::Stale) { return metadata; }
  if (!accepted->Terrain.ScopeCurrent(heightAt.TerrainScope)) {
    return CellSourceState::ScopeChanged;
  }
  if (metadata != CellSourceState::Current || !accepted->Terrain.IsComplete() ||
      accepted->Terrain.TerrainScopeRevision() == 0 || !heightAt.InspectCertificate) {
    return CellSourceState::Unknown;
  }
  switch (heightAt.InspectCertificate(accepted->Terrain)) {
    case Ground::TerrainCertificate::Validation::Current: return CellSourceState::Current;
    case Ground::TerrainCertificate::Validation::Unknown: return CellSourceState::Unknown;
    case Ground::TerrainCertificate::Validation::Stale: return CellSourceState::Stale;
    case Ground::TerrainCertificate::Validation::ScopeChanged: return CellSourceState::ScopeChanged;
    case Ground::TerrainCertificate::Validation::Pending: return CellSourceState::Pending;
  }
  std::unreachable();
}

bool StructureBuildQueue::ValidateResidentCellSource(const Ground::GroundStack &stack,
                                                     const Ground::BuildingField &footprints,
                                                     const HeightSource &heightAt,
                                                     uint32_t tile,
                                                     uint64_t sourceKey) {
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr || tile >= vectors->Tiles().size() || sourceKey == 0 ||
      QualifiedSourceKey(footprints, tile) != sourceKey) {
    return false;
  }
  if (InspectAcceptedSourceMetadata(*vectors, stack.Ways(), footprints, tile, sourceKey) !=
      CellSourceState::Current) {
    return false;
  }
  if (CertifiedAcceptedSourceCurrent(
          *vectors, stack.Ways(), footprints, heightAt, tile, sourceKey)) {
    return true;
  }
  HeightSource residentOnly = heightAt;
  residentOnly.Sample = {};
  residentOnly.CopyField = [&heightAt](Data::TileId at, Ground::HeightField::Block &into) {
    if (heightAt.CopyResidentField) { return heightAt.CopyResidentField(at, into); }
    return heightAt.ResidentField &&
           Ground::HeightField::SharesField(heightAt.ResidentField(at), at, into);
  };
  size_t deferred = 0;
  double resolutionMs = 0;
  return ValidateCellSource(
      stack, *vectors, footprints, residentOnly, tile, sourceKey, deferred, resolutionMs);
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

void StructureBuildQueue::RecycleOutput(StructureBuildTask &task) {
  auto output = task.TakeOutput();
  output->Tile.reset();
  IdleOut_.push_back(std::move(output));
}

void StructureBuildQueue::PostSlice(QueuedBuild &build) {
  if (build.Tasks == 0) {
    build.Task.Start(*Pool_, *Mesher_);
  } else {
    build.Task.Resume(*Pool_, *Mesher_);
  }
  build.Finished = false;
}

void StructureBuildQueue::DiscardFront(Ground::BuildingField &prints) {
  QueuedBuild &stale = Queue_.front();
  IdleRaw_.reserve(IdleRaw_.size() + 1u);
  IdleOut_.reserve(IdleOut_.size() + 1u);
  IdleScratch_.reserve(IdleScratch_.size() + 1u);
  if (stale.ReservationOwner == prints.ReservationOwner()) {
    if (stale.Replacement) {
      prints.RetryRefinement(stale.Task.Tile());
    } else {
      prints.Release(stale.Task.Tile());
    }
  }
  IdleRaw_.push_back(stale.Task.TakeRaw());
  RecycleOutput(stale.Task);
  IdleScratch_.push_back(stale.Task.TakeScratch());
  Queue_.pop_front();
  ++Discarded_;
}

void StructureBuildQueue::DiscardStale(const Ground::OsmField &vectors,
                                       Ground::BuildingField &prints,
                                       LongitudeLatitude eye,
                                       HeightSourceRevision heightSource,
                                       HeightRequirement heights,
                                       std::optional<LevelOfDetail> detail,
                                       BuildPurpose purpose) {
  while (!Queue_.empty() && (Queue_.front().ReservationOwner != prints.ReservationOwner() ||
                             !Queue_.front().Revision.Matches(
                                 vectors, prints, eye, heightSource, heights, detail, purpose))) {
    QueuedBuild &stale = Queue_.front();
    if (!stale.Finished) { stale.Finished = stale.Task.TakeCompletion(*Pool_); }
    if (!stale.Finished) { return; }
    DiscardFront(prints);
  }
}

void StructureBuildQueue::RetireCellBuilds(BuildPurpose purpose) {
  if (purpose != BuildPurpose::SourceGeometry) { return; }
  for (auto &batch : PreparedCells_) {
    if (!batch) { continue; }
    batch->Revoked = true;
    batch->Preparation->Cancel();
    const auto state = batch->Preparation->Advance();
    if (state != StructureSourcePreparation::State::Preparing && batch.use_count() == 1) {
      batch.reset();
    }
  }
  for (QueuedBuild &bake : CellQueue_) { bake.Task.RequestStop(); }
  while (!CellQueue_.empty()) {
    QueuedBuild &bake = CellQueue_.front();
    if (!bake.Finished) { bake.Finished = bake.Task.TakeCompletion(*Pool_); }
    if (!bake.Finished) { break; }
    IdleRaw_.push_back(bake.Task.TakeRaw());
    RecycleOutput(bake.Task);
    IdleScratch_.push_back(bake.Task.TakeScratch());
    CellQueue_.pop_front();
    ++Discarded_;
  }
  PinnedCellHeight_.reset();
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

void StructureBuildQueue::DeferPreparation(uint32_t tile,
                                           uint64_t sourceKey,
                                           uint64_t generation,
                                           const HeightSource &heightAt,
                                           bool permanent) {
  auto *found = std::ranges::find_if(DeferredPreparations_, [&](const auto &held) {
    return held.Tile == tile && held.SourceKey == sourceKey && held.Generation == generation &&
           held.Scope == heightAt.TerrainScope && held.Revision == heightAt.Revision;
  });
  if (found == DeferredPreparations_.end()) {
    found = DeferredPreparations_.begin() + static_cast<ptrdiff_t>(DeferredPreparationAt_);
    DeferredPreparationAt_ = (DeferredPreparationAt_ + 1u) % DeferredPreparations_.size();
    *found = {.Generation = generation,
              .Scope = heightAt.TerrainScope,
              .Revision = heightAt.Revision,
              .SourceKey = sourceKey,
              .Tile = tile};
  }
  found->Until = permanent ? std::numeric_limits<uint64_t>::max() : PreparationTick_ + found->Delay;
  found->Delay = std::min(found->Delay * 2u, uint64_t{16});
}

size_t StructureBuildQueue::QueuedCells() const {
  size_t count = CellQueue_.size();
  for (const auto &batch : PreparedCells_) {
    if (batch && !batch->Revoked) { count += batch->Count - batch->Next; }
  }
  return count;
}

size_t StructureBuildQueue::PostsCells(Ground::GroundStack &stack,
                                       Ground::BuildingField &footprints,
                                       LongitudeLatitude eye,
                                       const HeightSource &heightAt,
                                       std::span<const CellRequest> requests) {
  if (requests.empty() || requests.size() > 8 || Pool_ == nullptr || Mesher_ == nullptr ||
      !heightAt.CaptureFields || !footprints.Anchored()) {
    return 0;
  }
  const auto *vectors = stack.Vectors();
  const auto first = requests.front();
  const auto *accepted = footprints.InputOfTile(first.Tile);
  if (vectors == nullptr || first.Tile >= vectors->Tiles().size() || accepted == nullptr ||
      !accepted->Qualified || accepted->Heights.Tiles.empty() || accepted->Heights.Fallback ||
      accepted->Vector != VectorSource(*vectors, first.Tile) ||
      QualifiedSourceKey(footprints, first.Tile) != first.SourceKey ||
      StreetDigest(stack.Ways(), *vectors, first.Tile) != accepted->Bake.StreetDigest) {
    return 0;
  }
  for (const auto &deferred : DeferredPreparations_) {
    if (deferred.Tile == first.Tile && deferred.SourceKey == first.SourceKey &&
        deferred.Generation == vectors->Generation() && deferred.Scope == heightAt.TerrainScope &&
        deferred.Revision == heightAt.Revision && PreparationTick_ < deferred.Until) {
      return 0;
    }
  }
  uint64_t cells = 0;
  for (const auto request : requests) {
    if (request.Tile != first.Tile || request.SourceKey != first.SourceKey || request.Cell == 0 ||
        request.Cell > Generators::kStructureCellsPerTile ||
        request.Detail > LevelOfDetail::Massed || CellQueued(request)) {
      return 0;
    }
    const uint64_t bit = uint64_t{1} << (request.Cell - 1u);
    if ((cells & bit) != 0 || (accepted->OccupiedCells & bit) == 0) { return 0; }
    cells |= bit;
  }
  auto *const slot = std::ranges::find(PreparedCells_, nullptr);
  if (slot == PreparedCells_.end()) { return 0; }
  constexpr size_t bytesMost = size_t{8} * 1024u * 1024u;
  size_t receiptBytes = sizeof(PreparedCells) + sizeof(StructureSourcePreparation) +
                        accepted->Terrain.HeapBytes() +
                        accepted->Heights.Tiles.size() * sizeof(Ground::TileSpot) +
                        accepted->Sources.size() * sizeof(Data::TileSourceIdentity);
  const auto sourceBytes = [](const Data::TileSourceIdentity &source) {
    return source.SourceId.capacity() + source.Revision.capacity() + 2u;
  };
  for (const auto &source : accepted->Sources) { receiptBytes += sourceBytes(source); }
  if (accepted->Vector) { receiptBytes += sourceBytes(*accepted->Vector); }
  if (receiptBytes >= bytesMost) {
    DeferPreparation(first.Tile, first.SourceKey, vectors->Generation(), heightAt, true);
    return 0;
  }
  auto captured = heightAt.CaptureFields(accepted->Heights.Tiles, bytesMost - receiptBytes);
  if (!captured) {
    DeferPreparation(first.Tile,
                     first.SourceKey,
                     vectors->Generation(),
                     heightAt,
                     captured.error() != SourcedTerrainFields::CaptureError::MissingSource);
    ++Deferred_;
    return 0;
  }
  auto batch = std::make_shared<PreparedCells>();
  batch->Receipt = *accepted;
  std::ranges::copy(requests, batch->Requests.begin());
  batch->Count = requests.size();
  batch->Generation = vectors->Generation();
  batch->Scope = heightAt.TerrainScope;
  batch->Revision = heightAt.Revision;
  batch->Eye = eye;
  batch->Preparation = std::make_unique<StructureSourcePreparation>(
      *Pool_, accepted->Heights, std::move(*captured), bytesMost - receiptBytes);
  (void)batch->Preparation->Advance();
  *slot = std::move(batch);
  return requests.size();
}

void StructureBuildQueue::AdvancePreparedCells(const Ground::GroundStack &stack,
                                               const Ground::BuildingField &footprints,
                                               const HeightSource &heightAt) {
  for (auto &batch : PreparedCells_) {
    if (batch) { AdvancePreparedCell(batch, stack, footprints, heightAt); }
  }
}

void StructureBuildQueue::AdvancePreparedCell(std::shared_ptr<PreparedCells> &batch,
                                              const Ground::GroundStack &stack,
                                              const Ground::BuildingField &footprints,
                                              const HeightSource &heightAt) {
  const auto *vectors = stack.Vectors();
  const auto &request = batch->Requests.front();
  const auto *accepted = footprints.InputOfTile(request.Tile);
  const auto &receipt = batch->Receipt;
  const bool current =
      vectors != nullptr && accepted != nullptr && accepted->Qualified &&
      request.Tile < vectors->Tiles().size() && batch->Generation == vectors->Generation() &&
      batch->Revision == heightAt.Revision && batch->Scope == heightAt.TerrainScope &&
      accepted->Vector == receipt.Vector &&
      VectorSource(*vectors, request.Tile) == receipt.Vector &&
      accepted->Sources == receipt.Sources && accepted->Heights.Zoom == receipt.Heights.Zoom &&
      accepted->Heights.Fallback == receipt.Heights.Fallback &&
      std::ranges::equal(
          accepted->Heights.Tiles,
          receipt.Heights.Tiles,
          [](const auto a, const auto b) {
            return a.Zoom == b.Zoom && a.X == b.X && a.Y == b.Y;
          }) &&
      accepted->Bake.HeightRasterDigest == receipt.Bake.HeightRasterDigest &&
      accepted->Bake.StreetDigest == receipt.Bake.StreetDigest &&
      accepted->Bake.TileSpanM == receipt.Bake.TileSpanM &&
      footprints.TileSpanM() == receipt.Bake.TileSpanM &&
      accepted->OccupiedCells == receipt.OccupiedCells &&
      accepted->CellBounds == receipt.CellBounds &&
      accepted->CellMaxHeightM == receipt.CellMaxHeightM &&
      accepted->Terrain.IsComplete() == receipt.Terrain.IsComplete() &&
      accepted->Terrain.TerrainScopeRevision() == receipt.Terrain.TerrainScopeRevision() &&
      StreetDigest(stack.Ways(), *vectors, request.Tile) == receipt.Bake.StreetDigest &&
      std::ranges::equal(accepted->Terrain.Dependencies(), receipt.Terrain.Dependencies());
  if (!current) {
    batch->Revoked = true;
    batch->Preparation->Cancel();
  }
  const auto state = batch->Preparation->Advance();
  if (state == StructureSourcePreparation::State::Preparing) { return; }
  if (batch->Revoked) {
    if (batch.use_count() == 1) { batch.reset(); }
    return;
  }
  if (state != StructureSourcePreparation::State::Ready) {
    DeferPreparation(request.Tile, request.SourceKey, batch->Generation, heightAt, true);
    batch.reset();
    return;
  }
  const auto heights = batch->Preparation->Result();
  if (heights->RasterDigest() != receipt.Bake.HeightRasterDigest ||
      !std::ranges::equal(heights->Sources(), receipt.Sources)) {
    DeferPreparation(request.Tile, request.SourceKey, batch->Generation, heightAt, true);
    batch.reset();
    return;
  }
  if (!PinnedHeightsResident(*heights, heightAt)) {
    DeferPreparation(request.Tile, request.SourceKey, batch->Generation, heightAt, false);
    batch->Revoked = true;
    if (batch.use_count() == 1) { batch.reset(); }
    return;
  }
  if (batch->Next < batch->Count &&
      PostPreparedCell(
          stack, footprints, batch->Eye, heightAt, batch->Requests[batch->Next], heights, batch)) {
    ++batch->Next;
  }
  if (batch->Next == batch->Count && batch.use_count() == 1) { batch.reset(); }
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
  if (!stack.Ways().SourceDigest(*vectors, request.Tile)) {
    ++Deferred_;
    return false;
  }
  const auto &tile = vectors->Tiles()[request.Tile];
  const Ground::FeatureRun over{.From = tile.FirstFeature,
                                .To = static_cast<size_t>(tile.FirstFeature) + tile.FeatureCount};
  std::shared_ptr<const Ground::HeightField> heights;
  if (PinnedCellHeight_ && PinnedCellHeight_->Tile != request.Tile) { PinnedCellHeight_.reset(); }
  if (PinnedCellHeight_ && PinnedCellHeight_->SourceKey == request.SourceKey &&
      PinnedCellHeight_->Revision == heightAt.Revision &&
      (PinnedCellHeight_->Heights->Certificate().Dependencies().empty() ||
       PinnedHeightsResident(*PinnedCellHeight_->Heights, heightAt))) {
    heights = PinnedCellHeight_->Heights;
  }
  const bool pinMiss = !heights;
  double heightResolutionMs = 0.0;
  if (pinMiss && !ResolveHeights(*vectors,
                                 over,
                                 stack.FinestZoomOf(Data::DataKind::Elevation),
                                 heightAt,
                                 HeightRequirement::FineOnly,
                                 heights,
                                 {.Deferred = Deferred_, .DurationMs = heightResolutionMs})) {
    return false;
  }
  if (!heights) { return false; }
  SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, heightResolutionMs);
  const auto streetDigest = StreetDigest(stack.Ways(), *vectors, request.Tile);
  if (!streetDigest) {
    ++Deferred_;
    return false;
  }
  const uint64_t pinnedKey = StructureSourceKey({.Vector = VectorSource(*vectors, request.Tile),
                                                 .HeightSources = heights->Sources(),
                                                 .HeightDigest = heights->RasterDigest(),
                                                 .StreetDigest = *streetDigest,
                                                 .TileSpanM = footprints.TileSpanM(),
                                                 .FallbackHeights = heights->Fallback()});
  if (pinnedKey != request.SourceKey) { return false; }
  if (pinMiss && heights->HeapBytes() <= kPinnedCellHeightBytesMost) {
    PinnedCellHeight_ = {.Tile = request.Tile,
                         .SourceKey = request.SourceKey,
                         .Revision = heightAt.Revision,
                         .Heights = heights};
  }
  return PostPreparedCell(stack, footprints, eye, heightAt, request, std::move(heights), {});
}

bool StructureBuildQueue::PostPreparedCell(const Ground::GroundStack &stack,
                                           const Ground::BuildingField &footprints,
                                           LongitudeLatitude eye,
                                           const HeightSource &heightAt,
                                           CellRequest request,
                                           std::shared_ptr<const Ground::HeightField> heights,
                                           std::shared_ptr<const void> owner) {
  if (Queue_.size() + CellQueue_.size() >=
      std::min(static_cast<size_t>(Pool_->Threads()) * kBuildsPerThread, kCandidateWindow)) {
    return false;
  }
  const Ground::OsmField *vectors = stack.Vectors();
  const auto &tile = vectors->Tiles()[request.Tile];
  const Ground::FeatureRun over{.From = tile.FirstFeature,
                                .To = static_cast<size_t>(tile.FirstFeature) + tile.FeatureCount};
  const auto streetDigest = StreetDigest(stack.Ways(), *vectors, request.Tile);
  if (!streetDigest) { return false; }
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
  CellQueue_.push_back({.Revision = {.Vectors = vectors->Generation(),
                                     .HeightSource = heightAt.Revision,
                                     .TileSpanM = footprints.TileSpanM(),
                                     .Eye = eye,
                                     .RequestedDetail = request.Detail,
                                     .Purpose = BuildPurpose::ViewDetail},
                        .Task = StructureBuildTask(request.Tile,
                                                   std::move(raw),
                                                   std::move(heights),
                                                   std::move(output),
                                                   LentScratch(),
                                                   std::nullopt,
                                                   std::nullopt),
                        .StreetDigest = *streetDigest,
                        .SourceKey = request.SourceKey,
                        .Cell = request.Cell,
                        .ReservationOwner = std::move(owner)});
  PostSlice(CellQueue_.back());
  ++Posted_;
  return true;
}

void StructureBuildQueue::PrepareViewRefinement(Ground::BuildingField &prints,
                                                LongitudeLatitude eye,
                                                HeightRequirement requirement,
                                                BuildPurpose purpose,
                                                const std::function<bool(uint32_t)> &cellReady) {
  if (requirement == HeightRequirement::FineOnly && purpose == BuildPurpose::ViewDetail &&
      !cellReady && prints.RefinementComplete() && Queue_.empty() &&
      !AcceptedViewCurrent(prints, eye, cellReady)) {
    prints.BeginRefinement();
  }
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
  RetireCellBuilds(purpose);
  const Ground::OsmField &vectors = *stack.Vectors();
  if (!stack.Ways().SourceDigest(vectors, 0)) {
    ++Deferred_;
    return 0;
  }
  PrepareViewRefinement(prints, eye, requirement, purpose, cellReady);
  size_t posted = 0;
  const size_t inFlightMost =
      std::min(static_cast<size_t>(Pool_->Threads()) * kBuildsPerThread, kCandidateWindow);
  const int blockZoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  size_t remaining = candidatesMost;
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
      const RefinementSelection selected =
          SelectRefinement(prints, vectors, stack.Ways(), heights, groundStands, remaining);
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
    if (!InstrumentedHeightsCurrent(*heights, heightAt)) {
      ++Deferred_;
      break;
    }
    const auto streetDigest = StreetDigest(stack.Ways(), vectors, next->Tile);
    if (!streetDigest) {
      ++Deferred_;
      break;
    }
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
    const uint64_t sourceKey = StructureSourceKey({.Vector = VectorSource(vectors, next->Tile),
                                                   .HeightSources = heights->Sources(),
                                                   .HeightDigest = heights->RasterDigest(),
                                                   .StreetDigest = *streetDigest,
                                                   .TileSpanM = prints.TileSpanM(),
                                                   .FallbackHeights = heights->Fallback()});
    SlowestRawExtractionMs_ = std::max(
        SlowestRawExtractionMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - extractionAt)
            .count());
    std::unique_ptr<StructureBuildTask::Output> output = Borrowed(IdleOut_);
    *output = {};
    output->BakeMs = 0.0;
    output->LastRanges = 0;
    output->LastRangeMs = 0.0;
    output->FinalizationMs = 0.0;
    output->LastQueueMs = 0.0;
    output->LastTaskMs = 0.0;
    Queue_.push_back({.Revision = revision,
                      .Task = StructureBuildTask(next->Tile,
                                                 std::move(raw),
                                                 std::move(heights),
                                                 std::move(output),
                                                 LentScratch(),
                                                 std::nullopt,
                                                 std::nullopt),
                      .StreetDigest = *streetDigest,
                      .SourceKey = sourceKey,
                      .Replacement = replacement,
                      .ReservationOwner = prints.ReservationOwner()});
    const auto postingAt = std::chrono::steady_clock::now();
    PostSlice(Queue_.back());
    if (replacement) { prints.AdvanceRefinement(); }
    SlowestTaskPostingMs_ = std::max(
        SlowestTaskPostingMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postingAt)
            .count());
    ++Posted_;
    ++posted;
  }
  return posted;
}

bool StructureBuildQueue::WholeTileSourceCurrent(const Ground::GroundStack &stack,
                                                 const Ground::BuildingField &prints,
                                                 const HeightSource &heightAt,
                                                 const QueuedBuild &bake,
                                                 HeightRequirement heights,
                                                 Ground::TerrainCertificate &validated) {
  const Ground::OsmField &vectors = *stack.Vectors();
  const Ground::HeightField &captured = bake.Task.Heights();
  const bool certified = captured.Certificate().ScopeCurrent(heightAt.TerrainScope) &&
                         captured.Certificate().IsComplete() && heightAt.CertificateCurrent &&
                         heightAt.CertificateCurrent(captured.Certificate());
  bool current = bake.StreetDigest == StreetDigest(stack.Ways(), vectors, bake.Task.Tile());
  if (current && !certified) {
    const auto &record = vectors.Tiles()[bake.Task.Tile()];
    std::shared_ptr<const Ground::HeightField> resolved;
    double resolutionMs = 0.0;
    current = ResolveHeights(vectors,
                             {.From = record.FirstFeature,
                              .To = static_cast<size_t>(record.FirstFeature) + record.FeatureCount},
                             stack.FinestZoomOf(Data::DataKind::Elevation),
                             heightAt,
                             heights,
                             resolved,
                             {.Deferred = Deferred_, .DurationMs = resolutionMs});
    SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, resolutionMs);
    if (current && resolved) {
      const auto &certificate = resolved->Certificate();
      current = InstrumentedHeightsCurrent(*resolved, heightAt) &&
                StructureSourceKey({.Vector = record.Source,
                                    .HeightSources = resolved->Sources(),
                                    .HeightDigest = resolved->RasterDigest(),
                                    .StreetDigest = bake.StreetDigest,
                                    .TileSpanM = prints.TileSpanM(),
                                    .FallbackHeights = resolved->Fallback()}) == bake.SourceKey;
      if (current) { validated = certificate; }
    }
  } else if (current) {
    validated = captured.Certificate();
  }
  return current;
}

std::expected<std::vector<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextLandings(Ground::GroundStack &stack,
                                  Ground::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  const HeightSource &heightAt,
                                  size_t most,
                                  HeightRequirement heights,
                                  std::optional<LevelOfDetail> detail,
                                  BuildPurpose purpose) {
  std::vector<Landing> landings;
  if (Pool_ == nullptr || most == 0) { return landings; }
  RetireCellBuilds(purpose);
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr) { return landings; }
  DiscardStale(*vectors, prints, eye, heightAt.Revision, heights, detail, purpose);
  ResumeCompletedTasks();
  std::vector<Ground::TerrainCertificate> validated;
  validated.reserve(std::min(most, Queue_.size()));
  size_t count = 0;
  size_t printCount = 0;
  size_t spreadCount = 0;
  size_t acrossCount = 0;
  while (count < most && count < Queue_.size()) {
    QueuedBuild &bake = Queue_[count];
    if (!bake.Finished || bake.ReservationOwner != prints.ReservationOwner() ||
        !bake.Revision.Matches(
            *vectors, prints, eye, heightAt.Revision, heights, detail, purpose)) {
      break;
    }
    if (!bake.Task.Result().Status) {
      if (count == 0) { return std::unexpected(bake.Task.Result().Status.error()); }
      break;
    }
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { break; }
    Ground::TerrainCertificate certificate;
    const bool current =
        WholeTileSourceCurrent(stack, prints, heightAt, bake, heights, certificate);
    if (!current) {
      if (count != 0) { break; }
      DiscardFront(prints);
      continue;
    }
    validated.push_back(std::move(certificate));
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
    QueuedBuild &bake = Queue_[at];
    auto &completed = bake.Task.Result().Tile;
    if (!completed) { std::terminate(); }
    Generators::BakedTile &baked = *completed;
    if (!baked.Coordinates) {
      baked.Coordinates = std::make_shared<Ground::BuildingField::Geometry>();
      size_t source = 0;
      for (auto &footprint : baked.Prints) {
        const auto &structures = bake.Task.Raw().Structures;
        while (source < structures.size() &&
               structures[source].SourceFirst != footprint.FirstPoint) {
          ++source;
        }
        if (source == structures.size()) { std::terminate(); }
        footprint.FirstPoint = structures[source].LocalFirst;
        footprint.FirstHole = structures[source].FirstHole;
        ++source;
      }
    }
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    const std::optional<Data::TileSourceIdentity> vectorSource =
        VectorSource(*vectors, bake.Task.Tile());
    landings.push_back(
        {.Tile = bake.Task.Tile(),
         .Baked = &baked,
         .AnchorEcef = bake.Task.Raw().AnchorEcef,
         .SourceKey = bake.SourceKey,
         .Footprints = prints.PrepareAcceptance(
             bake.Task.Tile(),
             {.Coordinates = baked.Coordinates,
              .Prints = baked.Prints,
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
             QualifiedStructureHeights(
                 *vectors,
                 {.From = vectors->Tiles()[bake.Task.Tile()].FirstFeature,
                  .To = static_cast<size_t>(vectors->Tiles()[bake.Task.Tile()].FirstFeature) +
                        vectors->Tiles()[bake.Task.Tile()].FeatureCount},
                 bake.Task.Heights()),
             vectorSource,
             {.HeightRasterDigest = bake.Task.Heights().RasterDigest(),
              .StreetDigest = bake.StreetDigest,
              .FocalPx = bake.Revision.FocalPx,
              .TileSpanM = bake.Revision.TileSpanM,
              .Eye = bake.Revision.Eye},
             std::move(validated[at]),
             bake.Task.Heights().CaptureRequest())});
  }
  return landings;
}

bool StructureBuildQueue::ValidateCellLandingSource(const Ground::GroundStack &stack,
                                                    const Ground::BuildingField &footprints,
                                                    const HeightSource &heightAt,
                                                    const QueuedBuild &bake) {
  const auto *vectors = stack.Vectors();
  if (StreetDigest(stack.Ways(), *vectors, bake.Task.Tile()) == bake.StreetDigest &&
      PinnedHeightsResident(bake.Task.Heights(), heightAt)) {
    ++FastCellValidations_;
    return true;
  }
  if (bake.ReservationOwner) {
    DeferPreparation(bake.Task.Tile(), bake.SourceKey, bake.Revision.Vectors, heightAt, false);
    return false;
  }
  double resolutionMs = 0.0;
  if (!ValidateCellSource(stack,
                          *vectors,
                          footprints,
                          heightAt,
                          bake.Task.Tile(),
                          bake.SourceKey,
                          Deferred_,
                          resolutionMs)) {
    return false;
  }
  SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, resolutionMs);
  return true;
}

std::expected<std::optional<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextCellLanding(const Ground::GroundStack &stack,
                                     const Ground::BuildingField &footprints,
                                     const HeightSource &heightAt) {
  if (Pool_ == nullptr) { return std::nullopt; }
  ++PreparationTick_;
  AdvancePreparedCells(stack, footprints, heightAt);
  if (CellQueue_.empty()) { return std::nullopt; }
  QueuedBuild &bake = CellQueue_.front();
  const auto discard = [this, &bake] {
    IdleRaw_.push_back(bake.Task.TakeRaw());
    RecycleOutput(bake.Task);
    IdleScratch_.push_back(bake.Task.TakeScratch());
    CellQueue_.pop_front();
    ++Discarded_;
  };
  const Ground::OsmField *vectors = stack.Vectors();
  const auto *accepted = footprints.InputOfTile(bake.Task.Tile());
  const auto *batch = bake.ReservationOwner
                          ? static_cast<const PreparedCells *>(bake.ReservationOwner.get())
                          : nullptr;
  const bool current = (batch == nullptr || !batch->Revoked) && vectors != nullptr &&
                       accepted != nullptr && bake.Revision.Vectors == vectors->Generation() &&
                       bake.Revision.HeightSource == heightAt.Revision &&
                       QualifiedSourceKey(footprints, bake.Task.Tile()) == bake.SourceKey &&
                       (accepted->OccupiedCells & (uint64_t{1} << (bake.Cell - 1u))) != 0 &&
                       bake.Task.Tile() < vectors->Tiles().size() &&
                       VectorSource(*vectors, bake.Task.Tile()) == accepted->Vector;
  if (!current) {
    if (PinnedCellHeight_ && PinnedCellHeight_->Tile == bake.Task.Tile()) {
      PinnedCellHeight_.reset();
    }
    bake.Task.RequestStop();
    if (!bake.Finished) { bake.Finished = bake.Task.TakeCompletion(*Pool_); }
    if (bake.Finished) { discard(); }
    return std::nullopt;
  }
  ResumeCompletedTasks();
  if (!bake.Finished) { return std::nullopt; }
  if (!ValidateCellLandingSource(stack, footprints, heightAt, bake)) {
    if (PinnedCellHeight_ && PinnedCellHeight_->Tile == bake.Task.Tile()) {
      PinnedCellHeight_.reset();
    }
    discard();
    return std::nullopt;
  }
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
  RecycleOutput(bake.Task);
  IdleScratch_.push_back(bake.Task.TakeScratch());
  CellQueue_.pop_front();
  ++Landed_;
}

bool StructureBuildQueue::CellQueued(CellRequest request) const noexcept {
  for (const auto &batch : PreparedCells_) {
    if (!batch || batch->Revoked) { continue; }
    for (size_t at = batch->Next; at < batch->Count; ++at) {
      const auto &pending = batch->Requests[at];
      if (pending.Tile == request.Tile && pending.Cell == request.Cell &&
          pending.Detail == request.Detail && pending.SourceKey == request.SourceKey) {
        return true;
      }
    }
  }
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
    if (!landing.Footprints || !baked.Coordinates) { std::terminate(); }
    baked.Coordinates->Points = std::move(bake.Task.Raw().LatLon);
    baked.Coordinates->Rings = std::move(bake.Task.Raw().Holes);
    BakedMs_ += bake.Task.Result().BakeMs;
    SlowestBakeMs_ = std::max(SlowestBakeMs_, bake.Task.Result().BakeMs);
    assert(IdleRaw_.size() < IdleRaw_.capacity() && IdleOut_.size() < IdleOut_.capacity() &&
           IdleScratch_.size() < IdleScratch_.capacity());
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    const Ground::BuildingField::Baked product{.Coordinates = baked.Coordinates,
                                               .Prints = baked.Prints,
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
               {"cacheHit", bake.Task.Result().CacheHit ? 1 : 0},
               {"cacheReadMs", bake.Task.Result().CacheReadMs},
               {"cacheWriteMs", bake.Task.Result().CacheWriteMs},
               {"queued", static_cast<int>(Queue_.size() - 1)}});
    IdleRaw_.push_back(bake.Task.TakeRaw());
    RecycleOutput(bake.Task);
    IdleScratch_.push_back(bake.Task.TakeScratch());
    Queue_.pop_front();
    ++Landed_;
  }
}

void StructureBuildQueue::Clear() {
  for (const auto &batch : PreparedCells_) {
    if (batch) { batch->Preparation->Cancel(); }
  }
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
  PinnedCellHeight_.reset();
  PreparedCells_ = {};
  DeferredPreparations_ = {};
  DeferredPreparationAt_ = 0;
  PreparationTick_ = 0;
}

}
