#include <expected>
#include <exception>
#include <atomic>
#include "StructureBuildQueue.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <vector>
#include <string_view>
#include <utility>

#include "Log.h"
#include "Shape.h"
#include "OsmLayer.h"

namespace outshine {

namespace {

constexpr uint32_t kMostRingPoints = 512;
constexpr uint8_t kPolygonFeature = 3;
constexpr size_t kBuildsPerThread = 1;
constexpr double kBytesPerMB = 1024.0 * 1024.0;

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

void RawOf(const Ground::OsmField &vectors,
           const Ground::BuildingField &prints,
           const Ground::StreetField &streets,
           const Ground::TileWatermark::Next &next,
           LongitudeLatitude eye,
           Generators::RawTile &raw) {
  raw.LatLon.clear();
  raw.Structures.clear();
  raw.Ways.clear();
  raw.AnchorEcef = prints.Anchor();
  raw.Eye = eye;
  raw.FocalPx = prints.FocalPx();
  raw.TileSpanM = prints.TileSpanM();
  raw.Extent = vectors.Extent();
  raw.ClusterTriangles = Render::kClusterTriangles;
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  const std::span<const Ground::OsmField::Feature> feats = vectors.Features();
  const std::span<const double> points = vectors.Points();
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
      const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
      raw.LatLon.insert(raw.LatLon.end(),
                        points.begin() + static_cast<long>(ring.First) * 2,
                        points.begin() + static_cast<long>(ring.First + ring.Count) * 2);
      raw.Structures.push_back({.LocalFirst = local,
                                .PointCount = ring.Count,
                                .SourceFirst = ring.First,
                                .HeightM = heightM,
                                .Pitched = pitched});
    }
  }
}

bool Gathers(const Ground::GroundStream &ground,
             Ground::TileSpot spot,
             bool fineField,
             const StructureBuildQueue::HeightSource &heightAt,
             std::vector<Ground::HeightField::Block> &into) {
  if (std::ranges::any_of(into, [spot](const Ground::HeightField::Block &one) {
        return one.At.X == spot.X && one.At.Y == spot.Y;
      })) {
    return true;
  }
  Ground::HeightField::Block block;
  bool copied =
      fineField
          ? Ground::HeightField::CopiesField(ground.FieldOf({.Zoom = spot.Zoom,
                                                             .X = static_cast<uint32_t>(spot.X),
                                                             .Y = static_cast<uint32_t>(spot.Y)}),
                                             {.Zoom = spot.Zoom,
                                              .X = static_cast<uint32_t>(spot.X),
                                              .Y = static_cast<uint32_t>(spot.Y)},
                                             block)
          : Ground::HeightField::Copies(ground.BlockAt(spot), block);
  if (!copied && !fineField) {
    constexpr int side = 17;
    block.At = spot;
    block.Raster = {.Side = side, .Postings = side};
    block.Nodes.resize(static_cast<size_t>(side) * static_cast<size_t>(side));
    copied = true;
    const auto denominator = static_cast<double>(side - 1);
    for (int row = 0; row < side && copied; ++row) {
      for (int column = 0; column < side; ++column) {
        const auto at = Ground::TileFracToGeo(
            {.X = static_cast<double>(spot.X) + static_cast<double>(column) / denominator,
             .Y = static_cast<double>(spot.Y) + static_cast<double>(row) / denominator},
            spot.Zoom);
        const std::optional<double> height =
            heightAt({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg});
        if (!height) {
          copied = false;
          break;
        }
        block.Nodes[static_cast<size_t>(row) * static_cast<size_t>(side) +
                    static_cast<size_t>(column)] = static_cast<float>(*height);
      }
    }
  }
  if (!copied) { return false; }
  into.push_back(std::move(block));
  return true;
}

std::optional<std::vector<Ground::HeightField::Block>>
BlocksUnder(const Ground::GroundStream &ground,
            bool fineField,
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
    if (!Gathers(ground, spot, fineField, heightAt, blocks)) { complete = false; }
  }
  if (!complete) { return std::nullopt; }
  return blocks;
}

}

StructureBuildQueue::~StructureBuildQueue() {
  Clear();
}

bool StructureBuildQueue::Complete(const Ground::GroundStack &stack,
                                   const Ground::BuildingField &footprints) const {
  const Ground::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr || (Queue_.empty() && footprints.Ingested(*vectors));
}

size_t StructureBuildQueue::QueuedStructures() const {
  size_t count = 0;
  for (const QueuedBuild &bake : Queue_) {
    const size_t all = bake.Task.Raw().Structures.size();
    count += all > bake.BakedStructures ? all - bake.BakedStructures : 0;
  }
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
                                       LongitudeLatitude eye) {
  while (!Queue_.empty() && !Queue_.front().Revision.Matches(vectors, prints, eye)) {
    QueuedBuild &stale = Queue_.front();
    if (!stale.Finished) { stale.Finished = stale.Task.TakeCompletion(*Pool_); }
    if (!stale.Finished) { return; }
    IdleRaw_.reserve(IdleRaw_.size() + 1u);
    IdleOut_.reserve(IdleOut_.size() + 1u);
    IdleScratch_.reserve(IdleScratch_.size() + 1u);
    if (stale.Revision.Vectors == vectors.Generation()) { prints.Release(stale.Task.Tile()); }
    IdleRaw_.push_back(stale.Task.TakeRaw());
    IdleOut_.push_back(stale.Task.TakeOutput());
    IdleScratch_.push_back(stale.Task.TakeScratch());
    Queue_.pop_front();
    ++Discarded_;
  }
}

void StructureBuildQueue::ResumeCompletedTasks() {
  for (QueuedBuild &bake : Queue_) {
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
    if (bake.Finished && bake.Task.Result().Status && !bake.Task.Result().Tile) { PostSlice(bake); }
  }
}

size_t StructureBuildQueue::Posts(Ground::GroundStack &stack,
                                  Ground::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  const HeightSource &heightAt,
                                  size_t candidatesMost) {
  if (Pool_ == nullptr || Mesher_ == nullptr || stack.Vectors() == nullptr || !prints.Anchored()) {
    return 0;
  }
  const Ground::OsmField &vectors = *stack.Vectors();
  size_t posted = 0;
  const size_t inFlightMost =
      std::min(static_cast<size_t>(Pool_->Threads()) * kBuildsPerThread, kCandidateWindow);
  const int blockZoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  while (Queue_.size() < inFlightMost) {
    std::shared_ptr<const Ground::HeightField> heights;
    const auto groundStands = [&](Ground::FeatureRun over) {
      bool fallback = false;
      std::optional<std::vector<Ground::HeightField::Block>> blocks =
          BlocksUnder(stack.Ground(), true, blockZoom, vectors, over, heightAt);
      if (!blocks) {
        fallback = true;
        blocks = BlocksUnder(stack.Ground(), false, blockZoom, vectors, over, heightAt);
      }
      if (!blocks) {
        ++Deferred_;
        return false;
      }
      heights = Ground::HeightField::Of(blockZoom, std::move(*blocks), fallback);
      return true;
    };
    const std::optional<Ground::TileWatermark::Next> next =
        prints.Next(vectors, groundStands, candidatesMost);
    if (!next || !heights) { break; }
    const BakeRevision revision{.Vectors = vectors.Generation(),
                                .FocalPx = prints.FocalPx(),
                                .TileSpanM = prints.TileSpanM(),
                                .Eye = eye};
    prints.Take(next->Tile);
    std::unique_ptr<Generators::RawTile> raw = Borrowed(IdleRaw_);
    RawOf(vectors, prints, stack.Ways(), *next, eye, *raw);
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
             next->Tile, std::move(raw), std::move(heights), std::move(output), LentScratch())});
    PostSlice(Queue_.back());
    ++Posted_;
    ++posted;
  }
  return posted;
}

std::expected<std::vector<StructureBuildQueue::Landing>, Generators::StructureBakeError>
StructureBuildQueue::NextLandings(Ground::GroundStack &stack,
                                  Ground::BuildingField &prints,
                                  LongitudeLatitude eye,
                                  size_t most) {
  std::vector<Landing> landings;
  if (Pool_ == nullptr || most == 0) { return landings; }
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr) { return landings; }
  DiscardStale(*vectors, prints, eye);
  ResumeCompletedTasks();
  size_t count = 0;
  size_t printCount = 0;
  size_t spreadCount = 0;
  size_t acrossCount = 0;
  uint32_t largestTile = 0;
  while (count < most && count < Queue_.size()) {
    QueuedBuild &bake = Queue_[count];
    if (!bake.Finished || !bake.Revision.Matches(*vectors, prints, eye)) { break; }
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
    largestTile = std::max(largestTile, bake.Task.Tile());
    ++count;
  }
  if (count == 0) { return landings; }
  IdleRaw_.reserve(IdleRaw_.size() + count);
  IdleOut_.reserve(IdleOut_.size() + count);
  IdleScratch_.reserve(IdleScratch_.size() + count);
  prints.PreparesAcceptances({.Prints = printCount,
                              .Spread = spreadCount,
                              .Across = acrossCount,
                              .Tiles = count,
                              .LargestTile = largestTile});
  landings.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const QueuedBuild &bake = Queue_[at];
    const auto &completed = bake.Task.Result().Tile;
    if (!completed) { std::terminate(); }
    const Generators::BakedTile &baked = *completed;
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    landings.push_back(
        {.Tile = bake.Task.Tile(),
         .Baked = &baked,
         .AnchorEcef = bake.Task.Raw().AnchorEcef,
         .Footprints = prints.PrepareAcceptance(bake.Task.Tile(),
                                                {.Prints = baked.Prints,
                                                 .SeatSpreadM = baked.SeatSpreadM,
                                                 .AcrossM = baked.AcrossM,
                                                 .Triangles = triangles,
                                                 .OsmHeights = baked.OsmHeights,
                                                 .DefaultHeights = baked.DefaultHeights,
                                                 .Fronted = baked.Fronted})});
  }
  return landings;
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
    footprints.CommitAcceptance(std::move(landing.Footprints.value()),
                                *stack.Vectors(),
                                {.Prints = baked.Prints,
                                 .SeatSpreadM = baked.SeatSpreadM,
                                 .AcrossM = baked.AcrossM,
                                 .Triangles = triangles,
                                 .OsmHeights = baked.OsmHeights,
                                 .DefaultHeights = baked.DefaultHeights,
                                 .Fronted = baked.Fronted});
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
  Queue_.clear();
  IdleRaw_.clear();
  IdleOut_.clear();
  IdleScratch_.clear();
}

}
