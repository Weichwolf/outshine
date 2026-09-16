#include <expected>
#include <exception>
#include <atomic>
#include "StructureBakes.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <vector>
#include <string_view>
#include <utility>

#include "Heap.h"
#include "Log.h"
#include "Shape.h"
#include "OsmLayer.h"

namespace outshine {

namespace {

constexpr uint32_t kMostRingPoints = 512;
constexpr uint8_t kPolygonFeature = 3;
constexpr size_t kBakesPerThread = 1;
constexpr size_t kStructuresPerSlice = 32;
constexpr double kBytesPerMB = 1024.0 * 1024.0;

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

void RawOf(const Ground::OsmField &vectors,
           const Ground::BuildingField &prints,
           const Ground::TileWatermark::Next &next,
           Generators::RawTile &raw) {
  raw.LatLon.clear();
  raw.Structures.clear();
  raw.Ways.clear();
  raw.AnchorEcef = prints.Anchor();
  raw.AwayM = prints.AwayFromCentreM(vectors, next.Tile);
  raw.FocalPx = prints.FocalPx();
  raw.TileSpanM = prints.TileSpanM();
  raw.Extent = vectors.Extent();
  raw.ClusterTriangles = Render::kClusterTriangles;
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  const std::span<const Ground::OsmField::Feature> feats = vectors.Features();
  const std::span<const double> points = vectors.Points();
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
             std::vector<Ground::HeightField::Block> &into) {
  if (std::ranges::any_of(into, [spot](const Ground::HeightField::Block &one) {
        return one.At.X == spot.X && one.At.Y == spot.Y;
      })) {
    return true;
  }
  Ground::HeightField::Block block;
  const bool copied =
      fineField
          ? Ground::HeightField::CopiesField(ground.FieldOf({.Zoom = spot.Zoom,
                                                             .X = static_cast<uint32_t>(spot.X),
                                                             .Y = static_cast<uint32_t>(spot.Y)}),
                                             {.Zoom = spot.Zoom,
                                              .X = static_cast<uint32_t>(spot.X),
                                              .Y = static_cast<uint32_t>(spot.Y)},
                                             block)
          : Ground::HeightField::Copies(ground.BlockAt(spot), block);
  if (!copied) { return false; }
  into.push_back(std::move(block));
  return true;
}

std::optional<std::vector<Ground::HeightField::Block>>
BlocksUnder(const Ground::GroundStream &ground,
            bool fineField,
            int zoom,
            const Ground::OsmField &vectors,
            Ground::FeatureRun over) {
  const std::span<const Ground::OsmField::Feature> feats = vectors.Features();
  const int layer = vectors.Layer(Ground::OsmLayer::Buildings);
  std::vector<Ground::HeightField::Block> blocks;
  for (size_t at = over.From; at < over.To; ++at) {
    const Ground::OsmField::Feature &f = feats[at];
    if (f.Type != kPolygonFeature || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const Ground::TileSpot low =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MinLon, .LatitudeDeg = f.MaxLat}, zoom);
    const Ground::TileSpot high =
        Ground::HeightField::SpotOf({.LongitudeDeg = f.MaxLon, .LatitudeDeg = f.MinLat}, zoom);
    for (long y = low.Y; y <= high.Y; ++y) {
      for (long x = low.X; x <= high.X; ++x) {
        if (!Gathers(ground, {.Zoom = zoom, .X = x, .Y = y}, fineField, blocks)) {
          return std::nullopt;
        }
      }
    }
  }
  return blocks;
}

}

StructureBakes::~StructureBakes() {
  Clear();
}

bool StructureBakes::Complete(const Ground::GroundStack &stack) const {
  const Ground::OsmField *const vectors = stack.Vectors();
  return vectors == nullptr || (Queue_.empty() && stack.Footprints().Ingested(*vectors));
}

size_t StructureBakes::QueuedStructures() const {
  size_t count = 0;
  for (const Job &job : Queue_) { count += job.Raw->Structures.size(); }
  return count;
}

std::unique_ptr<MeshScratch> StructureBakes::LentScratch() {
  if (IdleScratch_.empty()) { return Mesher_->Scratch(); }
  std::unique_ptr<MeshScratch> one = std::move(IdleScratch_.back());
  IdleScratch_.pop_back();
  return one;
}

void StructureBakes::PostSlice(Job &job) {
  job.Finished = false;
  const Generators::RawTile *const raw = job.Raw.get();
  const Ground::HeightField *const under = job.Heights.get();
  const StructureMesher *const mesher = Mesher_;
  MeshScratch *const scratch = job.Scratch.get();
  Generators::StructureBakeProgress *const progress = job.Progress.get();
  Output *const out = job.Out.get();
  const std::shared_ptr<std::atomic_bool> stopping = job.Stopping;
  job.Handle = Pool_->Post([raw, under, mesher, scratch, progress, out, stopping] {
    const auto began = std::chrono::steady_clock::now();
    static const Heap::Tag kBakingTag("structure-bake");
    const Heap::Tagged baking(kBakingTag);
    const auto advanced = progress->Advance(
        *raw, *under, *mesher, *scratch, out->Tile, kStructuresPerSlice, stopping.get());
    if (!advanced) {
      out->Status = std::unexpected(advanced.error());
    } else {
      out->Complete = *advanced;
    }
    out->BakeMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
}

void StructureBakes::DiscardStale(const Ground::OsmField &vectors, Ground::BuildingField &prints) {
  while (!Queue_.empty() && !Queue_.front().Revision.Matches(vectors, prints)) {
    Job &stale = Queue_.front();
    if (!stale.Finished) { stale.Finished = Pool_->Done(stale.Handle); }
    if (!stale.Finished) { return; }
    IdleRaw_.reserve(IdleRaw_.size() + 1u);
    IdleOut_.reserve(IdleOut_.size() + 1u);
    IdleScratch_.reserve(IdleScratch_.size() + 1u);
    prints.Release(stale.Tile);
    IdleRaw_.push_back(std::move(stale.Raw));
    IdleOut_.push_back(std::move(stale.Out));
    IdleScratch_.push_back(std::move(stale.Scratch));
    Queue_.pop_front();
    ++Discarded_;
  }
}

void StructureBakes::ResumeSlices() {
  for (Job &job : Queue_) {
    if (!job.Finished) { job.Finished = Pool_->Done(job.Handle); }
    if (job.Finished && job.Out->Status && !job.Out->Complete) { PostSlice(job); }
  }
}

size_t StructureBakes::Posts(Ground::GroundStack &stack) {
  if (Pool_ == nullptr || Mesher_ == nullptr || stack.Vectors() == nullptr) { return 0; }
  const Ground::OsmField &vectors = *stack.Vectors();
  Ground::BuildingField &prints = stack.Footprints();
  size_t posted = 0;
  const size_t inFlightMost = static_cast<size_t>(Pool_->Threads()) * kBakesPerThread;
  const int blockZoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  while (Queue_.size() < inFlightMost) {
    std::shared_ptr<const Ground::HeightField> heights;
    const auto groundStands = [&](Ground::FeatureRun over) {
      std::optional<std::vector<Ground::HeightField::Block>> blocks =
          BlocksUnder(stack.Ground(), true, blockZoom, vectors, over);
      if (!blocks) {
        ++Deferred_;
        return false;
      }
      heights = Ground::HeightField::Of(blockZoom, std::move(*blocks));
      return true;
    };
    const std::optional<Ground::TileWatermark::Next> next = prints.Next(vectors, groundStands);
    if (!next || !heights) { break; }
    const BakeRevision revision{.Vectors = vectors.Generation(),
                                .FocalPx = prints.FocalPx(),
                                .TileSpanM = prints.TileSpanM()};
    prints.Take(next->Tile);
    Job job{.Tile = next->Tile,
            .Revision = revision,
            .Raw = Borrowed(IdleRaw_),
            .Heights = std::move(heights),
            .Out = Borrowed(IdleOut_),
            .Scratch = LentScratch(),
            .Progress = std::make_unique<Generators::StructureBakeProgress>(),
            .Stopping = std::make_shared<std::atomic_bool>(false)};
    RawOf(vectors, prints, *next, *job.Raw);
    job.Out->Status = {};
    job.Out->Complete = false;
    job.Out->BakeMs = 0.0;
    Queue_.push_back(std::move(job));
    PostSlice(Queue_.back());
    ++Posted_;
    ++posted;
  }
  return posted;
}

std::expected<std::vector<StructureBakes::Landing>, Generators::StructureBakeError>
StructureBakes::NextLandings(Ground::GroundStack &stack, size_t most) {
  std::vector<Landing> landings;
  if (Pool_ == nullptr || most == 0) { return landings; }
  const Ground::OsmField *vectors = stack.Vectors();
  if (vectors == nullptr) { return landings; }
  Ground::BuildingField &prints = stack.Footprints();
  DiscardStale(*vectors, prints);
  ResumeSlices();
  size_t count = 0;
  size_t printCount = 0;
  size_t spreadCount = 0;
  size_t acrossCount = 0;
  uint32_t largestTile = 0;
  while (count < most && count < Queue_.size()) {
    Job &job = Queue_[count];
    if (!job.Finished || !job.Revision.Matches(*vectors, prints)) { break; }
    if (!job.Out->Status) {
      if (count == 0) { return std::unexpected(job.Out->Status.error()); }
      break;
    }
    if (!job.Out->Complete) { break; }
    const Generators::BakedTile &baked = job.Out->Tile;
    printCount += baked.Prints.size();
    spreadCount += baked.SeatSpreadM.size();
    acrossCount += baked.AcrossM.size();
    largestTile = std::max(largestTile, job.Tile);
    ++count;
  }
  if (count == 0) { return landings; }
  IdleRaw_.reserve(IdleRaw_.size() + count);
  IdleOut_.reserve(IdleOut_.size() + count);
  IdleScratch_.reserve(IdleScratch_.size() + count);
  prints.PreparesAcceptances({.Prints = printCount,
                              .Spread = spreadCount,
                              .Across = acrossCount,
                              .LargestTile = largestTile});
  landings.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const Job &job = Queue_[at];
    const Generators::BakedTile &baked = job.Out->Tile;
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    landings.push_back(
        {.Tile = job.Tile,
         .Baked = &baked,
         .AnchorEcef = job.Raw->AnchorEcef,
         .Footprints = prints.PrepareAcceptance(job.Tile,
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

void StructureBakes::CommitsLandings(Ground::GroundStack &stack,
                                     std::span<Landing> landings) noexcept {
  for (Landing &landing : landings) {
    Job &job = Queue_.front();
    const Generators::BakedTile &baked = job.Out->Tile;
    assert(landing.Tile == job.Tile && landing.Baked == &baked && landing.Footprints);
    if (!landing.Footprints) { std::terminate(); }
    BakedMs_ += job.Out->BakeMs;
    SlowestBakeMs_ = std::max(SlowestBakeMs_, job.Out->BakeMs);
    assert(IdleRaw_.size() < IdleRaw_.capacity() && IdleOut_.size() < IdleOut_.capacity() &&
           IdleScratch_.size() < IdleScratch_.capacity());
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    stack.Footprints().CommitAcceptance(std::move(landing.Footprints.value()),
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
               {"total", static_cast<int>(stack.Footprints().Footprints().size())},
               {"osmHeight", stack.Footprints().OsmHeights()},
               {"defaultHeight", stack.Footprints().DefaultHeights()},
               {"vertsMB", static_cast<double>(baked.Built.UsedBytes()) / kBytesPerMB},
               {"lumped", baked.Lumped},
               {"blocks", baked.Blocks},
               {"unsupportedMeshes", static_cast<double>(baked.UnsupportedMeshes)},
               {"awayKm", job.Raw->AwayM / kMPerKm},
               {"bakeMs", job.Out->BakeMs},
               {"queued", static_cast<int>(Queue_.size() - 1)}});
    IdleRaw_.push_back(std::move(job.Raw));
    IdleOut_.push_back(std::move(job.Out));
    IdleScratch_.push_back(std::move(job.Scratch));
    Queue_.pop_front();
    ++Landed_;
  }
}

void StructureBakes::Clear() {
  for (const Job &job : Queue_) {
    job.Stopping->store(true, std::memory_order_relaxed);
    if (Pool_ != nullptr && !job.Finished && job.Handle != Tasks::kNoTask) {
      Pool_->Wait(job.Handle);
    }
  }
  Queue_.clear();
  IdleRaw_.clear();
  IdleOut_.clear();
  IdleScratch_.clear();
}

}
