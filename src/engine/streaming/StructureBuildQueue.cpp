#include "SurfacePreparation.h"
#include <expected>
#include <exception>
#include "StructureBuildQueue.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <bit>
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
#include <tuple>
#include <vector>
#include <variant>
#include <string>
#include <string_view>
#include <utility>

#include "Digest.h"
#include "Log.h"
#include "RenderGeometryView.h"
#include "OsmLayer.h"
#include "BuildingInputs.h"
#include "Geodesy.h"
#include "StructureSourceKey.h"

namespace outshine {

namespace {

constexpr uint8_t kPolygonFeature = 3;
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

[[nodiscard]] bool AcceptedViewCurrent(const ::outshine::Generators::Osm::BuildingField &prints,
                                       LongitudeLatitude eye,
                                       const std::function<bool(uint32_t)> &cellReady) {
  const auto inputs = prints.AcceptedInputs();
  const auto tiles = prints.AcceptedTiles();
  for (size_t at = 0; at < inputs.size(); ++at) {
    if (cellReady ? !cellReady(tiles[at]) : !EyeWithin(inputs[at].Bake.Eye, eye)) { return false; }
  }
  return true;
}

std::optional<Data::TileSourceIdentity>
VectorSource(const ::outshine::Generators::Osm::OsmField &vectors, uint32_t tile) {
  if (tile >= vectors.Tiles().size()) { return std::nullopt; }
  return vectors.Tiles()[tile].Source;
}

std::optional<uint64_t> StreetDigest(const ::outshine::Generators::Osm::StreetField &streets,
                                     const ::outshine::Generators::Osm::OsmField &vectors,
                                     uint32_t tile) {
  return streets.SourceDigest(vectors, tile);
}

void PrepareGeometry(Generators::BakedTile &baked, const Generators::RawTile &raw) {
  if (baked.Coordinates) { return; }
  baked.Coordinates = std::make_shared<Ground::BuildingGeometry>();
  baked.Coordinates->Origin = raw.SourceInputs.Origin;
  if (raw.SourceInputs.Objects) { baked.Coordinates->Sources.reserve(baked.Prints.size()); }
  size_t source = 0;
  for (auto &footprint : baked.Prints) {
    while (source < raw.Structures.size() &&
           raw.Structures[source].SourceFirst != footprint.FirstPoint) {
      ++source;
    }
    if (source == raw.Structures.size()) { std::terminate(); }
    footprint.FirstPoint = raw.Structures[source].LocalFirst;
    footprint.FirstHole = raw.Structures[source].FirstHole;
    if (raw.SourceInputs.Objects) {
      const auto id = raw.Structures[source].SourceId;
      baked.Coordinates->Sources.push_back({.Id = id.Id, .Kind = static_cast<uint8_t>(id.Kind)});
    }
    ++source;
  }
}

bool Gathers(Ground::TileSpot spot,
             bool fineField,
             const StructureBuildQueue::HeightSource &heightAt,
             std::vector<Ground::HeightField::Block> &into) {
  Ground::HeightField::Block block;
  const Data::TileId tile{
      .Zoom = spot.Zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
  bool copied = fineField && heightAt.PinField && heightAt.PinField(tile, block);
  if (!copied && !fineField && heightAt.Sample) {
    constexpr int side = 17;
    copied = Ground::HeightField::SamplesField(tile, side, heightAt.Sample, block);
  }
  if (!copied || !block.Certificate.ScopeCurrent(heightAt.Revision.Value)) { return false; }
  into.push_back(std::move(block));
  return true;
}

std::expected<std::vector<Ground::HeightField::Block>, Data::TileId>
BlocksUnder(bool fineField,
            int zoom,
            const ::outshine::Generators::Osm::OsmField &vectors,
            ::outshine::Generators::Osm::FeatureRun over,
            const StructureBuildQueue::HeightSource &heightAt) {
  const std::span<const ::outshine::Generators::Osm::OsmField::Feature> feats = vectors.Features();
  const int layer = vectors.Layer(::outshine::Generators::Osm::OsmLayer::Buildings);
  std::vector<Ground::TileSpot> spots;
  for (size_t at = over.From; at < over.To; ++at) {
    const ::outshine::Generators::Osm::OsmField::Feature &f = feats[at];
    if (f.Type != kPolygonFeature || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const Ground::TileSpot low =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MinLon, .LatitudeDeg = f.MaxLat}, zoom);
    const Ground::TileSpot high =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MaxLon, .LatitudeDeg = f.MinLat}, zoom);
    for (long y = low.Y; y <= high.Y; ++y) {
      for (long x = low.X; x <= high.X; ++x) {
        const Ground::TileSpot spot{.Zoom = zoom, .X = x, .Y = y};
        spots.push_back(spot);
      }
    }
  }
  const auto key = [](Ground::TileSpot spot) { return std::tuple(spot.X, spot.Y); };
  std::ranges::sort(spots, {}, key);
  spots.erase(std::ranges::unique(spots, {}, key).begin(), spots.end());
  std::vector<Ground::HeightField::Block> blocks;
  blocks.reserve(spots.size());
  for (const Ground::TileSpot spot : spots) {
    if (!Gathers(spot, fineField, heightAt, blocks)) {
      return std::unexpected(Data::TileId{.Zoom = spot.Zoom,
                                          .X = static_cast<uint32_t>(spot.X),
                                          .Y = static_cast<uint32_t>(spot.Y)});
    }
  }
  return blocks;
}

struct HeightResolutionStats {
  size_t &Deferred;
  double &DurationMs;
  StructureBuildQueue::HeightFailure *Failure = nullptr;
};

bool QualifiedStructureHeights(const ::outshine::Generators::Osm::OsmField &vectors,
                               ::outshine::Generators::Osm::FeatureRun over,
                               const Ground::HeightField &heights) {
  if (heights.Qualified()) { return true; }
  if (heights.Fallback() || !heights.Blocks().empty()) { return false; }
  const int layer = vectors.Layer(::outshine::Generators::Osm::OsmLayer::Buildings);
  return std::ranges::none_of(
      vectors.Features().subspan(over.From, over.To - over.From), [layer](const auto &feature) {
        return feature.Type == kPolygonFeature && std::cmp_equal(feature.Layer, layer);
      });
}

bool ResolveHeights(const ::outshine::Generators::Osm::OsmField &vectors,
                    ::outshine::Generators::Osm::FeatureRun over,
                    int blockZoom,
                    const StructureBuildQueue::HeightSource &heightAt,
                    StructureBuildQueue::HeightRequirement requirement,
                    std::shared_ptr<const Ground::HeightField> &heights,
                    HeightResolutionStats stats) {
  const auto began = std::chrono::steady_clock::now();
  using Failure = StructureBuildQueue::HeightFailure;
  if (stats.Failure != nullptr) { *stats.Failure = {}; }
  const auto deferred = [&](Failure::Reason reason, Data::TileId tile = {}) {
    if (stats.Failure != nullptr) { *stats.Failure = {.Cause = reason, .Tile = tile}; }
    ++stats.Deferred;
    stats.DurationMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    return false;
  };
  bool fallback = false;
  auto blocks = BlocksUnder(true, blockZoom, vectors, over, heightAt);
  if (!blocks && requirement == StructureBuildQueue::HeightRequirement::AllowFallback) {
    fallback = true;
    blocks = BlocksUnder(false, blockZoom, vectors, over, heightAt);
  }
  if (!blocks) { return deferred(Failure::Reason::Unavailable, blocks.error()); }
  for (const auto &block : *blocks) {
    const Data::TileId tile{.Zoom = block.At.Zoom,
                            .X = static_cast<uint32_t>(block.At.X),
                            .Y = static_cast<uint32_t>(block.At.Y)};
    if (requirement == StructureBuildQueue::HeightRequirement::FineOnly &&
        (block.Sources.empty() || block.MissingBoundary)) {
      return deferred(Failure::Reason::Unqualified, tile);
    }
  }
  auto pinned = Ground::HeightField::Of(blockZoom, std::move(*blocks), fallback);
  if (requirement == StructureBuildQueue::HeightRequirement::FineOnly &&
      !QualifiedStructureHeights(vectors, over, *pinned)) {
    return deferred(Failure::Reason::Unqualified);
  }
  heights = std::move(pinned);
  stats.DurationMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return true;
}

struct RefinementSelection {
  std::optional<::outshine::Generators::Osm::TileAdmission::Next> Next;
  bool Deferred = false;
};

template <typename GroundStands>
RefinementSelection SelectRefinement(::outshine::Generators::Osm::BuildingField &prints,
                                     const ::outshine::Generators::Osm::OsmField &vectors,
                                     const ::outshine::Generators::Osm::StreetField &streets,
                                     std::shared_ptr<const Ground::HeightField> &heights,
                                     const Generators::PreparedBuildingAssets::Basis &basis,
                                     const std::string &preparedAssetKey,
                                     GroundStands &&groundStands,
                                     size_t &remaining,
                                     bool checksView,
                                     const std::function<bool(uint32_t)> &cellReady) {
  if (remaining == 0) { return {.Next = std::nullopt, .Deferred = true}; }
  --remaining;
  const std::optional<uint32_t> tile = prints.RefinementTile();
  if (!tile) { return {.Next = std::nullopt, .Deferred = true}; }
  const std::span<const ::outshine::Generators::Osm::OsmField::Feature> features =
      vectors.Features();
  const auto first = std::ranges::lower_bound(
      features, *tile, std::ranges::less{}, &::outshine::Generators::Osm::OsmField::Feature::Tile);
  const auto last = std::ranges::upper_bound(
      features, *tile, std::ranges::less{}, &::outshine::Generators::Osm::OsmField::Feature::Tile);
  const size_t from = static_cast<size_t>(first - features.begin());
  const size_t to = static_cast<size_t>(last - features.begin());
  if (!groundStands({.From = from, .To = to}) || (!heights && !basis)) {
    return {.Next = std::nullopt, .Deferred = true};
  }
  const ::outshine::Generators::Osm::BuildingField::AcceptedInput *const accepted =
      prints.InputOfTile(*tile);
  const std::optional<Data::TileSourceIdentity> vectorSource = VectorSource(vectors, *tile);
  const bool sourceCurrent =
      accepted != nullptr && accepted->Qualified && accepted->Vector == vectorSource &&
      std::ranges::equal(accepted->Sources,
                         basis ? std::span(basis->HeightSources) : heights->Sources()) &&
      accepted->Bake.HeightRasterDigest ==
          (basis ? basis->HeightRasterDigest : heights->RasterDigest()) &&
      (!accepted->Bake.PreparedAssetKey.has_value()
           ? accepted->Bake.StreetDigest == StreetDigest(streets, vectors, *tile)
           : accepted->Bake.PreparedAssetKey == preparedAssetKey) &&
      accepted->Bake.TileSpanM == prints.TileSpanM();
  if (sourceCurrent && (!checksView || !cellReady || cellReady(*tile))) {
    prints.AdvanceRefinement();
    return {};
  }
  return {.Next = ::outshine::Generators::Osm::TileAdmission::Next{
              .From = from, .To = to, .Tile = *tile, .Found = true}};
}

}

void StructureBuildQueue::Opens(Tasks *pool,
                                const std::shared_ptr<const StructureMesher> &mesher,
                                const std::shared_ptr<JobAdmission> &admission,
                                const std::shared_ptr<JobAdmission> &contexts) {
  const bool changed = Pool_ != pool || Admission_ != admission || ContextAdmission_ != contexts ||
                       (Context_.Mesher && Context_.Mesher != mesher);
  if (Pool_ != nullptr && changed) { Clear(); }
  Pool_ = pool;
  if (Context_.Mesher != mesher) { Context_.Mesher = mesher; }
  if (Admission_ != admission) { Admission_ = admission; }
  if (ContextAdmission_ != contexts) { ContextAdmission_ = contexts; }
}

bool StructureBuildQueue::AdmitContext() {
  if (Context_.Admission) { return true; }
  if (Pool_ == nullptr || !ContextAdmission_) { return false; }
  auto lease = ContextAdmission_->TryAcquire();
  if (!lease) {
    ++ContextAdmissionDeferred_;
    return false;
  }
  Context_.Admission = std::make_shared<JobAdmission::Lease>(std::move(*lease));
  return true;
}

StructureBuildQueue::~StructureBuildQueue() {
  Clear();
}

std::optional<uint64_t> StructureBuildQueue::QualifiedSourceKey(
    const ::outshine::Generators::Osm::BuildingField &footprints, uint32_t tile) {
  const auto *accepted = footprints.InputOfTile(tile);
  if (accepted == nullptr || !accepted->Qualified) { return std::nullopt; }
  return accepted->SourceKey;
}

bool StructureBuildQueue::BakeRevision::Matches(
    const ::outshine::Generators::Osm::OsmField *vectors,
    const ::outshine::Generators::Osm::BuildingField &footprints,
    LongitudeLatitude eye,
    HeightSourceRevision heightSource,
    HeightRequirement heights,
    std::optional<LevelOfDetail> detail,
    BuildPurpose purpose,
    const Data::SourceObjects *inputObjects) const noexcept {
  return OwnsReservation(vectors, footprints, eye, heightSource, inputObjects) &&
         RequestedDetail == detail && Purpose == purpose &&
         (purpose == BuildPurpose::SourceGeometry ||
          (RequestedDetail && *RequestedDetail != LevelOfDetail::Fine) ||
          Generators::StructureSelectionDomain(Eye, Projection, EyeEcef)
              .Contains(eye, footprints.Projection(), footprints.EyeEcef())) &&
         (heights == HeightRequirement::AllowFallback || !FallbackHeights);
}

std::expected<bool, std::string>
StructureBuildQueue::PrepareOriginal(std::shared_ptr<const Generators::Osm::SourceSnapshot> source,
                                     Generators::Osm::StructurePolicy policy,
                                     int heightZoom) {
  return PrepareOriginal(
      source ? std::span(&source, 1) : std::span(&source, 0), policy, heightZoom);
}

std::expected<bool, std::string> StructureBuildQueue::PrepareOriginal(
    std::span<const std::shared_ptr<const Generators::Osm::SourceSnapshot>> sources,
    Generators::Osm::StructurePolicy policy,
    int heightZoom) {
  if (!sources.empty()) {
    if (Pool_ == nullptr || !ContextAdmission_) {
      return std::unexpected("original structures require an open worker pool");
    }
    if (!AdmitContext()) { return false; }
  }
  return PrepareOriginalInputs(
      Generators::Osm::StructurePreparation::SourceInputs(sources.begin(), sources.end()),
      policy,
      heightZoom);
}

std::expected<bool, std::string> StructureBuildQueue::PrepareOriginal(
    std::span<const std::shared_ptr<const Generators::Osm::StructureCell>> cells, int heightZoom) {
  if (!cells.empty()) {
    if (Pool_ == nullptr || !ContextAdmission_) {
      return std::unexpected("original structures require an open worker pool");
    }
    if (!AdmitContext()) { return false; }
  }
  return PrepareOriginalInputs(
      Generators::Osm::StructurePreparation::CellInputs(cells.begin(), cells.end()),
      {},
      heightZoom);
}

std::expected<bool, std::string>
StructureBuildQueue::PrepareOriginalInputs(Generators::Osm::StructurePreparation::Inputs inputs,
                                           Generators::Osm::StructurePolicy policy,
                                           int heightZoom) {
  const auto sources = std::visit(
      [](const auto &values) {
        return std::vector<std::shared_ptr<const void>>(values.begin(), values.end());
      },
      inputs);
  if (sources.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("original building cells exceed product address admission");
  }
  if (Context_.OriginalPreparation) {
    const bool same = std::ranges::equal(Context_.PreparingOriginals, sources) &&
                      Context_.PreparingOriginalHeightZoom == heightZoom;
    if (!same) { Context_.OriginalPreparation->Cancel(); }
    const auto phase = Context_.OriginalPreparation->Poll();
    if (phase == Generators::Osm::StructurePreparation::Phase::Working) { return false; }
    if (same) {
      if (phase == Generators::Osm::StructurePreparation::Phase::Failed) {
        return std::unexpected(std::string(Context_.OriginalPreparation->Error()));
      }
      const auto products = Context_.OriginalPreparation->Products();
      Context_.Originals.assign(products.begin(), products.end());
      const auto tiles = Context_.OriginalPreparation->HeightTiles();
      Context_.OriginalHeightTiles.assign(tiles.begin(), tiles.end());
      Context_.OriginalHeightZoom = heightZoom;
    }
    Context_.OriginalPreparation.reset();
    Context_.PreparingOriginals.clear();
    Context_.PreparingOriginalHeightZoom = -1;
  }
  if (sources.empty()) {
    Context_.Originals.clear();
    Context_.OriginalHeightTiles.clear();
    Context_.OriginalHeightZoom = -1;
    return true;
  }
  if (Context_.Originals.size() == sources.size() && Context_.OriginalHeightZoom == heightZoom &&
      std::ranges::equal(Context_.Originals, sources, {}, [](const auto &product) {
        return product.Input->SourceInputs.Archive.lock();
      })) {
    return true;
  }
  if (Pool_ == nullptr) {
    return std::unexpected("original structures require an open worker pool");
  }
  Context_.PreparingOriginals.assign(sources.begin(), sources.end());
  Context_.PreparingOriginalHeightZoom = heightZoom;
  Context_.OriginalPreparation = std::make_unique<Generators::Osm::StructurePreparation>(
      *Pool_, std::move(inputs), policy, heightZoom, Context_.Admission);
  return false;
}

const Data::SourceObjects *StructureBuildQueue::InputObjectsFor(uint32_t tile) const noexcept {
  return tile < Context_.Originals.size()
             ? Context_.Originals[tile].Input->SourceInputs.Objects.get()
             : nullptr;
}

std::expected<std::span<const Data::TileId>, std::string>
StructureBuildQueue::OriginalHeightTiles(int zoom) const {
  if (!HasOriginal()) { return std::span<const Data::TileId>(); }
  if (zoom != Context_.OriginalHeightZoom) {
    return std::unexpected("original building terrain demand requires preparation at this zoom");
  }
  return std::span<const Data::TileId>(Context_.OriginalHeightTiles);
}

bool StructureBuildQueue::Complete(const Ground::SurfacePreparation &stack,
                                   const ::outshine::Generators::Osm::BuildingField &footprints,
                                   LongitudeLatitude eye,
                                   const std::function<bool(uint32_t)> &cellReady) const {
  if (HasOriginal()) {
    return Context_.Queue.empty() && SourcesComplete(stack, footprints) &&
           AcceptedViewCurrent(footprints, eye, cellReady);
  }
  const ::outshine::Generators::Osm::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr ||
         (Context_.Queue.empty() && footprints.RefinementComplete() &&
          AcceptedViewCurrent(footprints, eye, cellReady) && footprints.Ingested(*vectors));
}

bool StructureBuildQueue::SourcesComplete(
    const Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &footprints) const {
  if (HasOriginal()) {
    if (HasPendingSourceChanges(footprints) ||
        footprints.AcceptedTiles().size() != Context_.Originals.size()) {
      return false;
    }
    for (size_t tile = 0; tile < Context_.Originals.size(); ++tile) {
      const auto *input = footprints.InputOfTile(static_cast<uint32_t>(tile));
      if (input == nullptr || !input->Qualified || !input->Coordinates ||
          input->Coordinates->Origin.Provenance !=
              Context_.Originals[tile].Input->SourceInputs.Origin.Provenance ||
          input->Coordinates->Origin.Selection !=
              Context_.Originals[tile].Input->SourceInputs.Origin.Selection) {
        return false;
      }
    }
    return true;
  }
  return !HasPendingSourceChanges(footprints) && QualifiedSources(stack, footprints);
}

bool StructureBuildQueue::HasPendingSourceChanges(
    const ::outshine::Generators::Osm::BuildingField &footprints) const {
  return std::ranges::any_of(Context_.Queue, [&](const QueuedBuild &build) {
    if (build.Revision.Purpose != BuildPurpose::ViewDetail ||
        build.ReservationOwner != footprints.ReservationOwner()) {
      return true;
    }
    const auto accepted = QualifiedSourceKey(footprints, build.Task.Tile());
    return !accepted || *accepted != build.SourceKey;
  });
}

bool StructureBuildQueue::QualifiedSources(
    const Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &footprints) {
  const ::outshine::Generators::Osm::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr ||
         (footprints.SourceRefinementComplete() && footprints.Ingested(*vectors) &&
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
  countRemaining(Context_.Queue);
  return count;
}

std::unique_ptr<MeshScratch> StructureBuildQueue::LentScratch() {
  if (Context_.IdleScratch.empty()) { return Context_.Mesher->Scratch(); }
  std::unique_ptr<MeshScratch> one = std::move(Context_.IdleScratch.back());
  Context_.IdleScratch.pop_back();
  return one;
}

void StructureBuildQueue::RecycleTask(StructureBuildTask &task) {
  Context_.IdleRaw.push_back(task.TakeRaw());
  auto output = task.TakeOutputForReuse();
  Context_.IdleOut.push_back(std::move(output));
  Context_.IdleScratch.push_back(task.TakeScratch());
  if (!task.Retire(*Pool_)) { std::terminate(); }
}

std::optional<JobAdmission::Lease> StructureBuildQueue::TryAdmitBuild() {
  assert(Pool_ != nullptr && Admission_);
  const int maximum = std::min(Pool_->Threads(), Admission_->Maximum());
  if (Context_.Queue.size() >= static_cast<size_t>(maximum)) { return std::nullopt; }
  auto admission = Admission_->TryAcquire();
  if (!admission) { ++AdmissionDeferred_; }
  return admission;
}

void StructureBuildQueue::PostSlice(QueuedBuild &build) {
  if (build.Tasks != 0 && !build.Task.CanResume()) { return; }
  if (build.Tasks == 0) {
    build.Task.Start(*Pool_, Context_.Mesher, Context_.Admission);
  } else {
    build.Task.Resume(*Pool_);
  }
  build.Finished = false;
}

void StructureBuildQueue::DiscardFront(::outshine::Generators::Osm::BuildingField &prints) {
  QueuedBuild &stale = Context_.Queue.front();
  Context_.IdleRaw.reserve(Context_.IdleRaw.size() + 1u);
  Context_.IdleOut.reserve(Context_.IdleOut.size() + 1u);
  Context_.IdleScratch.reserve(Context_.IdleScratch.size() + 1u);
  if (stale.ReservationOwner == prints.ReservationOwner()) {
    if (stale.Replacement) {
      if (!stale.Task.Raw().SourceInputs.Objects) {
        using Purpose = Generators::Osm::BuildingField::RefinementPurpose;
        const auto source = QualifiedSourceKey(prints, stale.Task.Tile());
        const bool view = stale.Revision.Purpose == BuildPurpose::ViewDetail && source &&
                          *source == stale.SourceKey;
        prints.RetryRefinement(stale.Task.Tile(), view ? Purpose::View : Purpose::Sources);
      }
    } else {
      prints.Release(stale.Task.Tile());
    }
  }
  RecycleTask(stale.Task);
  Context_.Queue.pop_front();
  ++Discarded_;
}

void StructureBuildQueue::DiscardStale(const ::outshine::Generators::Osm::OsmField *vectors,
                                       ::outshine::Generators::Osm::BuildingField &prints,
                                       LongitudeLatitude eye,
                                       HeightSourceRevision heightSource,
                                       HeightRequirement heights,
                                       std::optional<LevelOfDetail> detail,
                                       BuildPurpose purpose) {
  while (!Context_.Queue.empty() &&
         (Context_.Queue.front().ReservationOwner != prints.ReservationOwner() ||
          !Context_.Queue.front().Revision.Matches(
              vectors,
              prints,
              eye,
              heightSource,
              heights,
              detail,
              purpose,
              InputObjectsFor(Context_.Queue.front().Task.Tile())))) {
    QueuedBuild &stale = Context_.Queue.front();
    if (!stale.Finished) { stale.Finished = stale.Task.TakeCompletion(); }
    if (!stale.Finished) { return; }
    DiscardFront(prints);
  }
}

void StructureBuildQueue::AdvanceQueuedTasks(const ::outshine::Generators::Osm::OsmField *vectors,
                                             ::outshine::Generators::Osm::BuildingField &prints,
                                             LongitudeLatitude eye,
                                             HeightSourceRevision heightSource,
                                             HeightRequirement heights,
                                             std::optional<LevelOfDetail> detail,
                                             BuildPurpose purpose) {
  PollCompletedTasks();
  DiscardStale(vectors, prints, eye, heightSource, heights, detail, purpose);
  for (QueuedBuild &bake : Context_.Queue) {
    if (bake.Finished && bake.Task.Result().Status && !bake.Task.Result().Tile &&
        bake.ReservationOwner == prints.ReservationOwner() &&
        bake.Revision.Matches(vectors,
                              prints,
                              eye,
                              heightSource,
                              heights,
                              detail,
                              purpose,
                              InputObjectsFor(bake.Task.Tile()))) {
      PostSlice(bake);
    }
  }
}

void StructureBuildQueue::PollCompletedTasks() {
  for (const auto &entry : Context_.NativeLookups) {
    auto &lookup = *entry.second;
    if (lookup.Working && lookup.Finished.load(std::memory_order_acquire)) {
      lookup.Working = false;
    }
  }
  const auto collect = [this](std::deque<QueuedBuild> &queue) {
    for (QueuedBuild &bake : queue) {
      if (!bake.Finished) {
        bake.Finished = bake.Task.TakeCompletion();
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
    }
  };
  collect(Context_.Queue);
}

size_t StructureBuildQueue::PostsOriginal(uint32_t tile,
                                          Ground::SurfacePreparation &stack,
                                          ::outshine::Generators::Osm::BuildingField &prints,
                                          LongitudeLatitude eye,
                                          const HeightSource &heightAt,
                                          HeightRequirement requirement,
                                          std::optional<LevelOfDetail> detail,
                                          BuildPurpose purpose) {
  if (Pool_ == nullptr || Context_.Mesher == nullptr || !prints.Anchored() ||
      !Context_.Queue.empty()) {
    return 0;
  }
  const auto &original = Context_.Originals[tile].Input;
  const auto *accepted = prints.InputOfTile(tile);
  if (accepted != nullptr && accepted->Coordinates &&
      accepted->Coordinates->Origin.Provenance == original->SourceInputs.Origin.Provenance &&
      accepted->Coordinates->Origin.Selection == original->SourceInputs.Origin.Selection &&
      accepted->HeightRevision != 0 && accepted->HeightRevision == heightAt.Revision.Value &&
      accepted->Qualified &&
      (purpose == BuildPurpose::SourceGeometry ||
       Generators::StructureSelectionDomain(
           accepted->Bake.Eye, accepted->Bake.Projection, accepted->Bake.EyeEcef)
           .Contains(eye, prints.Projection(), prints.EyeEcef()))) {
    return 0;
  }
  const int zoom = stack.TerrainRenderZoom();
  if (zoom != Context_.OriginalHeightZoom) {
    ++Deferred_;
    return 0;
  }
  const auto &tiles = Context_.Originals[tile].HeightTiles;
  auto admission = TryAdmitBuild();
  if (!admission) { return 0; }
  const auto gather = [&](bool fine) -> std::optional<std::vector<Ground::HeightField::Block>> {
    std::vector<Ground::HeightField::Block> blocks;
    blocks.reserve(tiles.size());
    for (const auto heightTile : tiles) {
      if (!Gathers({.Zoom = zoom,
                    .X = static_cast<long>(heightTile.X),
                    .Y = static_cast<long>(heightTile.Y)},
                   fine,
                   heightAt,
                   blocks)) {
        return std::nullopt;
      }
    }
    return blocks;
  };
  auto blocks = gather(true);
  const bool fallback = !blocks;
  if (!blocks && requirement == HeightRequirement::AllowFallback) { blocks = gather(false); }
  if (!blocks) {
    ++Deferred_;
    return 0;
  }
  auto heights = Ground::HeightField::Of(zoom, std::move(*blocks), fallback);
  if (requirement == HeightRequirement::FineOnly && !original->Structures.empty() &&
      !heights->Qualified()) {
    ++Deferred_;
    return 0;
  }
  auto raw = Borrowed(Context_.IdleRaw);
  *raw = *original;
  raw->AnchorEcef = prints.Anchor();
  raw->Eye = eye;
  raw->EyeEcef = prints.EyeEcef();
  raw->Projection = prints.Projection();
  raw->TileSpanM = prints.TileSpanM();
  raw->RequestedDetail = detail;
  const uint64_t sourceKey = StructureSourceKey({.Vector = std::nullopt,
                                                 .HeightSources = heights->Sources(),
                                                 .HeightDigest = heights->RasterDigest(),
                                                 .TileSpanM = prints.TileSpanM(),
                                                 .FallbackHeights = heights->Fallback(),
                                                 .Origin = &raw->SourceInputs.Origin});
  const BakeRevision revision{.HeightSource = heightAt.Revision,
                              .Projection = prints.Projection(),
                              .TileSpanM = prints.TileSpanM(),
                              .Eye = eye,
                              .EyeEcef = prints.EyeEcef(),
                              .RequestedDetail = detail,
                              .Purpose = purpose,
                              .FallbackHeights = heights->Fallback(),
                              .InputObjects = raw->SourceInputs.Objects.get()};
  if (accepted == nullptr) { prints.Take(tile); }
  auto output = Borrowed(Context_.IdleOut);
  *output = {};
  Context_.Queue.push_back({.Revision = revision,
                            .Task = StructureBuildTask(std::move(*admission),
                                                       tile,
                                                       std::move(raw),
                                                       std::move(heights),
                                                       std::move(output),
                                                       LentScratch()),
                            .SourceKey = sourceKey,
                            .Replacement = accepted != nullptr,
                            .ReservationOwner = prints.ReservationOwner()});
  PostSlice(Context_.Queue.back());
  ++Posted_;
  return 1;
}

size_t StructureBuildQueue::Posts(Ground::SurfacePreparation &stack,
                                  ::outshine::Generators::Osm::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  const HeightSource &heightAt,
                                  size_t candidatesMost,
                                  HeightRequirement requirement,
                                  std::optional<LevelOfDetail> detail,
                                  BuildPurpose purpose,
                                  const std::function<bool(uint32_t)> &cellReady) {
  if (candidatesMost == 0 || Pool_ == nullptr || Context_.Mesher == nullptr) { return 0; }
  if (!AdmitContext()) { return 0; }
  AdvanceQueuedTasks(stack.Vectors(), prints, eye, heightAt.Revision, requirement, detail, purpose);
  if (HasOriginal()) {
    for (size_t tile = 0; tile < Context_.Originals.size(); ++tile) {
      if (const auto posted = PostsOriginal(static_cast<uint32_t>(tile),
                                            stack,
                                            prints,
                                            eye,
                                            heightAt,
                                            requirement,
                                            detail,
                                            purpose)) {
        return posted;
      }
    }
    return 0;
  }
  return PostsVectors(
      stack, prints, eye, heightAt, candidatesMost, requirement, detail, purpose, cellReady);
}

StructureBuildQueue::QueuedBuild
StructureBuildQueue::VectorBuild(JobAdmission::Lease admission,
                                 Ground::SurfacePreparation &stack,
                                 const ::outshine::Generators::Osm::BuildingField &prints,
                                 ::outshine::Generators::Osm::TileAdmission::Next next,
                                 BakeRevision revision,
                                 VectorSelection selected,
                                 bool replacement) {
  auto raw = Borrowed(Context_.IdleRaw);
  const auto extractionAt = std::chrono::steady_clock::now();
  if (selected.Basis) {
    *raw = {};
    raw->Eye = revision.Eye;
    raw->EyeEcef = revision.EyeEcef;
    raw->Projection = revision.Projection;
    raw->RequestedDetail = revision.RequestedDetail;
    raw->ClusterTriangles = Render::kClusterTriangles;
  } else {
    raw->ClusterTriangles = Render::kClusterTriangles;
    Generators::Osm::PrepareBuildingInputs(*stack.Vectors(),
                                           prints,
                                           stack.Ways(),
                                           next,
                                           revision.Eye,
                                           revision.RequestedDetail,
                                           std::nullopt,
                                           *raw);
  }
  SlowestRawExtractionMs_ = std::max(
      SlowestRawExtractionMs_,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - extractionAt)
          .count());
  auto output = Borrowed(Context_.IdleOut);
  *output = {};
  output->CacheHit = selected.Basis != nullptr;
  output->CacheReadMs = selected.ReadMs;
  auto task = selected.Basis ? StructureBuildTask(std::move(admission),
                                                  next.Tile,
                                                  std::move(selected.Basis),
                                                  std::move(raw),
                                                  std::move(output),
                                                  LentScratch())
                             : StructureBuildTask(std::move(admission),
                                                  next.Tile,
                                                  std::move(raw),
                                                  std::move(selected.Heights),
                                                  std::move(output),
                                                  LentScratch());
  if (!selected.Key.empty() && !revision.FallbackHeights &&
      (task.HeightQualified() || task.Raw().Structures.empty())) {
    task.UseNativeAssets(stack.BuildingAssets(), selected.Key, selected.RequestKey);
    if (revision.Purpose == BuildPurpose::SourceGeometry && !revision.RequestedDetail) {
      task.UseInitialCells();
    }
  }
  const uint64_t sourceKey =
      StructureSourceKey({.Vector = VectorSource(*stack.Vectors(), next.Tile),
                          .HeightSources = task.HeightSources(),
                          .HeightDigest = task.HeightRasterDigest(),
                          .StreetDigest = selected.StreetDigest,
                          .TileSpanM = prints.TileSpanM(),
                          .FallbackHeights = revision.FallbackHeights,
                          .Origin = &task.Raw().SourceInputs.Origin,
                          .PreparedAssetKey = task.PreparedAssetKey()});
  if (!selected.RequestKey.empty()) { Context_.NativeLookups.erase(selected.RequestKey); }
  return {.Revision = revision,
          .Task = std::move(task),
          .StreetDigest = selected.StreetDigest,
          .SourceKey = sourceKey,
          .Replacement = replacement,
          .ReservationOwner = prints.ReservationOwner()};
}

bool StructureBuildQueue::ResolveVectorHeights(const Generators::Osm::OsmField &vectors,
                                               Generators::Osm::FeatureRun over,
                                               int zoom,
                                               const HeightSource &source,
                                               HeightRequirement requirement,
                                               VectorSelection &selected,
                                               double &durationMs) {
  const uint32_t tile = vectors.Features()[over.From].Tile;
  if (Context_.PendingHeights && (Context_.PendingHeights->Generation != vectors.Generation() ||
                                  Context_.PendingHeights->Revision != source.Revision)) {
    Context_.PendingHeights.reset();
  }
  if (!Context_.PendingHeights) {
    Context_.PendingHeights.emplace(PendingVectorHeights{.Tile = tile,
                                                         .Generation = vectors.Generation(),
                                                         .Revision = source.Revision,
                                                         .Bytes = 0,
                                                         .Fields = {}});
  }
  if (Context_.PendingHeights->Tile != tile) {
    ++Deferred_;
    return false;
  }
  HeightSource retained = source;
  retained.PinField = [this, &source](Data::TileId at, Ground::HeightField::Block &into) {
    auto &pending = *Context_.PendingHeights;
    const auto found =
        std::ranges::find(pending.Fields, at, [](const auto &entry) { return entry.first; });
    if (found != pending.Fields.end()) {
      into = found->second;
      return true;
    }
    if (!source.PinField || !source.PinField(at, into) ||
        !into.Certificate.ScopeCurrent(source.Revision.Value)) {
      return false;
    }
    constexpr size_t bytesMost = size_t{512} * 1024 * 1024;
    const size_t bytes =
        sizeof(Ground::HeightField::Block) +
        (into.Terrain ? into.Terrain->HeapBytes() : into.Nodes.capacity() * sizeof(float));
    if (bytes > bytesMost - pending.Bytes) {
      Context_.NativeFailure = Generators::StructureBakeErrorKind::ArtifactCapacityExceeded;
      return false;
    }
    pending.Fields.emplace_back(at, into);
    pending.Bytes += bytes;
    return true;
  };
  const bool ready = ResolveHeights(
      vectors,
      over,
      zoom,
      retained,
      requirement,
      selected.Heights,
      {.Deferred = Deferred_, .DurationMs = durationMs, .Failure = &Context_.LastHeightFailure});
  if (ready) { Context_.PendingHeights.reset(); }
  return ready;
}

bool StructureBuildQueue::SelectVectorInputs(
    Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &prints,
    ::outshine::Generators::Osm::FeatureRun over,
    const HeightSource &heightAt,
    HeightRequirement requirement,
    VectorSelection &selected,
    double &durationMs) {
  selected = {};
  const auto &vectors = *stack.Vectors();
  if (over.From < vectors.Features().size() && heightAt.Revision.Value != 0 &&
      !SelectNativeBasis(stack, prints, vectors.Features()[over.From].Tile, selected)) {
    ++Deferred_;
    return false;
  }
  if (selected.Basis && (requirement != HeightRequirement::FineOnly ||
                         selected.Basis->Sources.empty() || selected.Basis->HeightQualified)) {
    return true;
  }
  selected.Basis.reset();
  if (over.From < vectors.Features().size() &&
      (!stack.Ways().Ingested(vectors) ||
       !stack.Ways().SourceDigest(vectors, vectors.Features()[over.From].Tile))) {
    stack.RequestStreets();
    ++Deferred_;
    return false;
  }
  if (over.From == over.To) {
    return ResolveHeights(
        vectors,
        over,
        stack.TerrainRenderZoom(),
        heightAt,
        requirement,
        selected.Heights,
        {.Deferred = Deferred_, .DurationMs = durationMs, .Failure = &Context_.LastHeightFailure});
  }
  return ResolveVectorHeights(
      vectors, over, stack.TerrainRenderZoom(), heightAt, requirement, selected, durationMs);
}

void StructureBuildQueue::PrepareViewRefinement(
    ::outshine::Generators::Osm::BuildingField &prints,
    LongitudeLatitude eye,
    HeightRequirement requirement,
    BuildPurpose purpose,
    const std::function<bool(uint32_t)> &cellReady) const {
  if (requirement == HeightRequirement::FineOnly && purpose == BuildPurpose::ViewDetail &&
      prints.RefinementComplete() && Context_.Queue.empty() &&
      !AcceptedViewCurrent(prints, eye, cellReady)) {
    prints.BeginRefinement(Generators::Osm::BuildingField::RefinementPurpose::View);
  }
}

size_t StructureBuildQueue::PostsVectors(Ground::SurfacePreparation &stack,
                                         ::outshine::Generators::Osm::BuildingField &prints,
                                         LongitudeLatitude eye,
                                         const HeightSource &heightAt,
                                         size_t candidatesMost,
                                         HeightRequirement requirement,
                                         std::optional<LevelOfDetail> detail,
                                         BuildPurpose purpose,
                                         const std::function<bool(uint32_t)> &cellReady) {
  if (Pool_ == nullptr || Context_.Mesher == nullptr || stack.Vectors() == nullptr ||
      !prints.Anchored()) {
    return 0;
  }
  const ::outshine::Generators::Osm::OsmField &vectors = *stack.Vectors();
  PrepareViewRefinement(prints, eye, requirement, purpose, cellReady);
  size_t posted = 0;
  size_t remaining = candidatesMost;
  while (auto admission = TryAdmitBuild()) {
    VectorSelection selected;
    double heightResolutionMs = 0.0;
    const auto groundStands = [&](::outshine::Generators::Osm::FeatureRun over) {
      return SelectVectorInputs(
          stack, prints, over, heightAt, requirement, selected, heightResolutionMs);
    };
    const auto selectionAt = std::chrono::steady_clock::now();
    std::optional<::outshine::Generators::Osm::TileAdmission::Next> next;
    bool replacement = false;
    if (requirement == HeightRequirement::FineOnly && !prints.RefinementComplete()) {
      const RefinementSelection refinement = SelectRefinement(prints,
                                                              vectors,
                                                              stack.Ways(),
                                                              selected.Heights,
                                                              selected.Basis,
                                                              selected.Key,
                                                              groundStands,
                                                              remaining,
                                                              purpose == BuildPurpose::ViewDetail,
                                                              cellReady);
      if (refinement.Deferred) { break; }
      if (!refinement.Next) { continue; }
      next = refinement.Next;
      replacement = true;
    } else {
      next = prints.Next(vectors, groundStands, candidatesMost);
    }
    SlowestCandidateSelectionMs_ = std::max(
        SlowestCandidateSelectionMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - selectionAt)
            .count());
    SlowestHeightResolutionMs_ = std::max(SlowestHeightResolutionMs_, heightResolutionMs);
    if (!next) { break; }
    const auto streetDigest = selected.Basis ? std::optional(selected.StreetDigest)
                                             : StreetDigest(stack.Ways(), vectors, next->Tile);
    if (!streetDigest) {
      ++Deferred_;
      break;
    }
    const BakeRevision revision{.Vectors = vectors.Generation(),
                                .HeightSource = heightAt.Revision,
                                .Projection = prints.Projection(),
                                .TileSpanM = prints.TileSpanM(),
                                .Eye = eye,
                                .EyeEcef = prints.EyeEcef(),
                                .RequestedDetail = detail,
                                .Purpose = purpose,
                                .FallbackHeights = selected.Basis ? selected.Basis->FallbackHeights
                                                                  : selected.Heights->Fallback()};
    if (!replacement) { prints.Take(next->Tile); }
    selected.StreetDigest = *streetDigest;
    Context_.Queue.push_back(VectorBuild(
        std::move(*admission), stack, prints, *next, revision, std::move(selected), replacement));
    const auto postingAt = std::chrono::steady_clock::now();
    PostSlice(Context_.Queue.back());
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

bool StructureBuildQueue::WholeTileSourceCurrent(const Ground::SurfacePreparation &stack,
                                                 const HeightSource &heightAt,
                                                 const QueuedBuild &bake,
                                                 HeightRequirement heights) const {
  if (bake.Task.FromPreparedInputs()) {
    return (heights != HeightRequirement::FineOnly || bake.Task.PreparedStructureCount() == 0 ||
            bake.Task.HeightQualified()) &&
           bake.Revision.HeightSource == heightAt.Revision &&
           (bake.Task.PreparedAssetKey().has_value() ||
            bake.StreetDigest == StreetDigest(stack.Ways(), *stack.Vectors(), bake.Task.Tile()));
  }
  const auto &captured = bake.Task.Heights();
  if (heights == HeightRequirement::FineOnly && !bake.Task.Raw().Structures.empty() &&
      !captured.Qualified()) {
    return false;
  }
  if (bake.Revision.HeightSource.Value == 0 && !captured.Blocks().empty()) {
    std::vector<Ground::HeightField::Block> blocks;
    blocks.reserve(captured.Blocks().size());
    for (const auto &block : captured.Blocks()) {
      if (!Gathers(block.At, !captured.Fallback(), heightAt, blocks)) { return false; }
    }
    const auto resolved = Ground::HeightField::Of(
        captured.CaptureRequest().Zoom, std::move(blocks), captured.Fallback());
    if (resolved->RasterDigest() != captured.RasterDigest() ||
        !std::ranges::equal(resolved->Sources(), captured.Sources())) {
      return false;
    }
  }
  if (bake.Task.Raw().SourceInputs.Objects) {
    return bake.Task.Raw().SourceInputs.Objects.get() == InputObjectsFor(bake.Task.Tile()) &&
           bake.Task.Raw().SourceInputs.Origin.Selection ==
               Context_.Originals[bake.Task.Tile()].Input->SourceInputs.Origin.Selection;
  }
  return bake.StreetDigest == StreetDigest(stack.Ways(), *stack.Vectors(), bake.Task.Tile());
}

bool StructureBuildQueue::RetryMissingNativeInputs(
    const StructureBuildTask::Output &output,
    size_t preceding,
    ::outshine::Generators::Osm::BuildingField &prints) {
  if (output.Status || preceding != 0) { return false; }
  const auto *kind = std::get_if<Generators::StructureBakeErrorKind>(&output.Status.error());
  if (kind == nullptr || *kind != Generators::StructureBakeErrorKind::NativeInputsMissing) {
    return false;
  }
  DiscardFront(prints);
  return true;
}

std::expected<std::vector<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextLandings(Ground::SurfacePreparation &stack,
                                  ::outshine::Generators::Osm::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  const HeightSource &heightAt,
                                  size_t most,
                                  HeightRequirement heights,
                                  std::optional<LevelOfDetail> detail,
                                  BuildPurpose purpose) {
  if (Context_.NativeFailure) { return std::unexpected(*Context_.NativeFailure); }
  std::vector<Landing> landings;
  if (Pool_ == nullptr || most == 0) { return landings; }
  const ::outshine::Generators::Osm::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr && !HasOriginal()) { return landings; }
  AdvanceQueuedTasks(vectors, prints, eye, heightAt.Revision, heights, detail, purpose);
  size_t count = 0;
  while (count < std::min(most, Context_.Queue.size())) {
    QueuedBuild &bake = Context_.Queue[count];
    if (!bake.Finished || bake.ReservationOwner != prints.ReservationOwner() ||
        !bake.Revision.Matches(vectors,
                               prints,
                               eye,
                               heightAt.Revision,
                               heights,
                               detail,
                               purpose,
                               InputObjectsFor(bake.Task.Tile()))) {
      break;
    }
    if (RetryMissingNativeInputs(bake.Task.Result(), count, prints)) { continue; }
    if (!bake.Task.Result().Status) {
      if (count == 0) { return std::unexpected(bake.Task.Result().Status.error()); }
      break;
    }
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { break; }
    const bool current = WholeTileSourceCurrent(stack, heightAt, bake, heights);
    if (!current) {
      if (count != 0) { break; }
      DiscardFront(prints);
      continue;
    }
    CollectInitialCell(bake);

    ++count;
    if (bake.InitialCells) { break; }
  }
  return PrepareLandings(prints, vectors, count);
}

void StructureBuildQueue::CollectInitialCell(QueuedBuild &bake) {
  auto &completed = bake.Task.Result().Tile;
  assert(completed);
  if (const uint64_t cells = bake.Task.InitialCells(); cells != 0) {
    if (!bake.InitialCells) { bake.InitialCells.emplace(cells); }
    PrepareGeometry(*completed, bake.Task.Raw());
    bake.InitialCells->Collect(*completed);
  }
}

::outshine::Generators::Osm::BuildingField::Baked
StructureBuildQueue::ContactMetadata(const QueuedBuild &bake) noexcept {
  const auto &completed = bake.Task.Result().Tile;
  assert(completed);
  const auto &baked = bake.InitialCells ? bake.InitialCells->Metadata() : *completed;
  return {.Coordinates = baked.Coordinates,
          .Prints = baked.Prints,
          .SeatSpreadM = baked.SeatSpreadM,
          .AcrossM = baked.AcrossM,
          .OccupiedCells = baked.OccupiedCells,
          .CellBounds = baked.CellBounds,
          .CellMaxHeightM = baked.CellMaxHeightM,
          .CellShellErrorM = baked.CellShellErrorM,
          .Triangles = bake.InitialCells
                           ? bake.InitialCells->Triangles()
                           : (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u,
          .OsmHeights = baked.OsmHeights,
          .DefaultHeights = baked.DefaultHeights,
          .Fronted = baked.Fronted};
}

std::vector<StructureBuildQueue::Landing>
StructureBuildQueue::PrepareLandings(::outshine::Generators::Osm::BuildingField &prints,
                                     const ::outshine::Generators::Osm::OsmField *vectors,
                                     size_t count) {
  std::vector<Landing> landings;
  if (count == 0) { return landings; }
  Context_.IdleRaw.reserve(Context_.IdleRaw.size() + count);
  Context_.IdleOut.reserve(Context_.IdleOut.size() + count);
  Context_.IdleScratch.reserve(Context_.IdleScratch.size() + count);
  prints.PreparesAcceptances(count);
  landings.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    landings.push_back(PrepareLanding(Context_.Queue[at], prints, vectors));
  }
  return landings;
}

StructureBuildQueue::Landing
StructureBuildQueue::PrepareLanding(QueuedBuild &bake,
                                    ::outshine::Generators::Osm::BuildingField &prints,
                                    const ::outshine::Generators::Osm::OsmField *vectors) {
  auto &completed = bake.Task.Result().Tile;
  if (!completed) { std::terminate(); }
  Generators::BakedTile &baked = *completed;
  PrepareGeometry(baked, bake.Task.Raw());
  const bool complete = !bake.InitialCells || bake.InitialCells->Remaining() == 0;
  Landing landing{.Tile = bake.Task.Tile(),
                  .Baked = &baked,
                  .AnchorEcef = bake.Task.Raw().AnchorEcef,
                  .SourceKey = bake.SourceKey,
                  .InitialCells = bake.InitialCells ? bake.InitialCells->Cells() : 0,
                  .SourceComplete = complete,
                  .Footprints = std::nullopt};
  if (!complete) { return landing; }
  if (bake.Replacement && bake.Revision.Purpose == BuildPurpose::ViewDetail &&
      landing.SourceKey != 0 && QualifiedSourceKey(prints, landing.Tile) == landing.SourceKey) {
    return landing;
  }
  const std::optional<Data::TileSourceIdentity> vectorSource =
      baked.Coordinates->Origin.Provenance ? std::nullopt
                                           : VectorSource(*vectors, bake.Task.Tile());
  const bool qualified = [&] {
    if (bake.Task.FromPreparedInputs()) {
      return bake.Task.PreparedStructureCount() == 0 || bake.Task.HeightQualified();
    }
    if (baked.Coordinates->Origin.Provenance) {
      return bake.Task.Raw().Structures.empty() || bake.Task.HeightQualified();
    }
    return QualifiedStructureHeights(
        *vectors,
        {.From = vectors->Tiles()[bake.Task.Tile()].FirstFeature,
         .To = static_cast<size_t>(vectors->Tiles()[bake.Task.Tile()].FirstFeature) +
               vectors->Tiles()[bake.Task.Tile()].FeatureCount},
        bake.Task.Heights());
  }();
  landing.Footprints =
      prints.PrepareAcceptance(bake.Task.Tile(),
                               ContactMetadata(bake),
                               bake.Task.HeightSources(),
                               qualified,
                               vectorSource,
                               {.HeightRasterDigest = bake.Task.HeightRasterDigest(),
                                .StreetDigest = bake.StreetDigest,
                                .Projection = bake.Revision.Projection,
                                .TileSpanM = bake.Revision.TileSpanM,
                                .Eye = bake.Revision.Eye,
                                .EyeEcef = bake.Revision.EyeEcef,
                                .PreparedAssetKey = bake.Task.PreparedAssetKey().transform(
                                    [](const auto key) { return std::string(key); })},
                               bake.Revision.HeightSource.Value,
                               bake.Task.HeightRequest());
  return landing;
}

void StructureBuildQueue::CommitFootprints(
    QueuedBuild &bake,
    Landing &landing,
    ::outshine::Generators::Osm::BuildingField &footprints) noexcept {
  const auto product = ContactMetadata(bake);
  if (!product.Coordinates) { std::terminate(); }
  if (!bake.Task.FromPreparedInputs()) {
    auto &completed = bake.Task.Result().Tile;
    assert(completed && completed->Coordinates);
    completed->Coordinates->OwnCoordinates(std::move(bake.Task.Raw().LatLon),
                                           std::move(bake.Task.Raw().Holes));
  }
  if (!landing.Footprints) {
    assert(bake.Replacement && bake.Revision.Purpose == BuildPurpose::ViewDetail &&
           QualifiedSourceKey(footprints, landing.Tile) == landing.SourceKey);
    return;
  }
  if (bake.Replacement) {
    footprints.ReplaceAcceptance(std::move(landing.Footprints.value()), product);
  } else {
    footprints.CommitAcceptance(std::move(landing.Footprints.value()), product);
  }
}

void StructureBuildQueue::CommitsLandings(::outshine::Generators::Osm::BuildingField &footprints,
                                          std::span<Landing> landings) noexcept {
  for (Landing &landing : landings) {
    QueuedBuild &bake = Context_.Queue.front();
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { std::terminate(); }
    const Generators::BakedTile &child = *completed;
    assert(landing.Tile == bake.Task.Tile() && landing.Baked == &child);
    BakedMs_ += bake.Task.Result().BakeMs;
    SlowestBakeMs_ = std::max(SlowestBakeMs_, bake.Task.Result().BakeMs);
    if (!landing.SourceComplete) {
      assert(bake.InitialCells && !landing.Footprints && bake.InitialCells->Remaining() != 0);
      const uint32_t cell =
          static_cast<uint32_t>(std::countr_zero(bake.InitialCells->Remaining())) + 1u;
      bake.Task.SelectNextCell(cell);
      bake.Tasks = 0;
      PostSlice(bake);
      continue;
    }
    const Generators::BakedTile &baked = bake.InitialCells ? bake.InitialCells->Metadata() : child;
    CommitFootprints(bake, landing, footprints);
    assert(Context_.IdleRaw.size() < Context_.IdleRaw.capacity() &&
           Context_.IdleOut.size() < Context_.IdleOut.capacity() &&
           Context_.IdleScratch.size() < Context_.IdleScratch.capacity());
    Log::Info(LogTag::World,
              "buildings",
              {{"added", static_cast<int>(baked.Prints.size())},
               {"total", static_cast<int>(footprints.FootprintCount())},
               {"osmHeight", footprints.OsmHeights()},
               {"defaultHeight", footprints.DefaultHeights()},
               {"vertsMB",
                static_cast<double>(bake.InitialCells ? bake.InitialCells->GeometryBytes()
                                                      : baked.Built.UsedBytes()) /
                    kBytesPerMB},
               {"lumped", baked.Lumped},
               {"blocks", baked.Blocks},
               {"unsupportedMeshes", static_cast<double>(baked.UnsupportedMeshes)},
               {"bakeMs", bake.Task.Result().BakeMs},
               {"cacheHit", bake.Task.Result().CacheHit ? 1 : 0},
               {"cacheReadMs", bake.Task.Result().CacheReadMs},
               {"cacheWriteMs", bake.Task.Result().CacheWriteMs},
               {"queued", static_cast<int>(Context_.Queue.size() - 1)}});
    RecycleTask(bake.Task);
    Context_.Queue.pop_front();
    ++Landed_;
  }
}

void StructureBuildQueue::Clear() {
  if (!Context_.Admission) { return; }
  if (Context_.OriginalPreparation) { Context_.OriginalPreparation->Cancel(); }
  for (const auto &entry : Context_.NativeLookups) {
    entry.second->Stopping.store(true, std::memory_order_relaxed);
  }
  for (QueuedBuild &bake : Context_.Queue) {
    if (!bake.Task.Retire(*Pool_)) { std::terminate(); }
  }

  struct Handoff {
    std::unique_ptr<BuildContext> Value;
  };

  const auto retired =
      std::make_shared<Handoff>(std::make_unique<BuildContext>(std::move(Context_)));
  Context_ = {};
  Context_.Mesher = retired->Value->Mesher;
  if (!Pool_->PostDetached([retired] {
        auto context = std::move(retired->Value);
        context.reset();
      })) {
    std::terminate();
  }
}

}
