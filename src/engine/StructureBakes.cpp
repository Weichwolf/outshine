#include "StructureBakes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <string_view>
#include <utility>

#include "Log.h"
#include "OsmLayer.h"

namespace outshine {

namespace {

constexpr uint32_t kMostRingPoints = 512;
constexpr uint8_t kPolygonFeature = 3;
constexpr size_t kBakesPerThread = 2;
constexpr double kBytesPerMB = 1024.0 * 1024.0;

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

std::shared_ptr<Generators::RawTile> RawOf(const Ground::OsmField &vectors,
                                           const Ground::BuildingField &prints,
                                           const Ground::TileWatermark::Next &next) {
  auto raw = std::make_shared<Generators::RawTile>();
  raw->AnchorEcef = prints.Anchor();
  raw->AwayM = prints.AwayFromCentreM(vectors, next.Tile);
  raw->FocalPx = prints.FocalPx();
  raw->TileSpanM = prints.TileSpanM();
  raw->Extent = vectors.Extent();
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
      const auto local = static_cast<uint32_t>(raw->LatLon.size() / 2);
      raw->LatLon.insert(raw->LatLon.end(),
                         points.begin() + static_cast<long>(ring.First) * 2,
                         points.begin() + static_cast<long>(ring.First + ring.Count) * 2);
      raw->Structures.push_back({.LocalFirst = local,
                                 .PointCount = ring.Count,
                                 .SourceFirst = ring.First,
                                 .HeightM = heightM,
                                 .Pitched = pitched});
    }
  }
  return raw;
}

bool Gathers(const GroundQuery &ground,
             Ground::TileSpot spot,
             std::vector<Ground::HeightField::Block> &into) {
  if (std::ranges::any_of(into, [spot](const Ground::HeightField::Block &one) {
        return one.At.X == spot.X && one.At.Y == spot.Y;
      })) {
    return true;
  }
  Ground::HeightField::Block block;
  if (!Ground::HeightField::Copies(ground.BlockAt(spot), block)) { return false; }
  into.push_back(std::move(block));
  return true;
}

std::optional<std::vector<Ground::HeightField::Block>> BlocksUnder(const GroundQuery &ground,
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
        if (!Gathers(ground, {.Zoom = zoom, .X = x, .Y = y}, blocks)) { return std::nullopt; }
      }
    }
  }
  return blocks;
}

} // namespace

size_t StructureBakes::Posts(Ground::GroundStack &stack) {
  if (Pool_ == nullptr || Mesher_ == nullptr || stack.Vectors() == nullptr) { return 0; }
  const Ground::OsmField &vectors = *stack.Vectors();
  Ground::BuildingField &prints = stack.Footprints();
  size_t posted = 0;
  const size_t inFlightMost = static_cast<size_t>(Pool_->Threads()) * kBakesPerThread;
  const int blockZoom = stack.Ground().BlockZoom();
  while (Queue_.size() < inFlightMost) {
    std::shared_ptr<const Ground::HeightField> heights;
    const auto groundStands = [&](Ground::FeatureRun over) {
      std::optional<std::vector<Ground::HeightField::Block>> blocks =
          BlocksUnder(stack.Ground(), blockZoom, vectors, over);
      if (!blocks) {
        ++Deferred_;
        return false;
      }
      heights = Ground::HeightField::Of(blockZoom, std::move(*blocks));
      return true;
    };
    const std::optional<Ground::TileWatermark::Next> next = prints.Next(vectors, groundStands);
    if (!next || !heights) { break; }
    prints.Take(next->Tile);
    std::shared_ptr<Generators::RawTile> raw = RawOf(vectors, prints, *next);
    auto out = std::make_shared<Generators::BakedTile>();
    const StructureMesher *const mesher = Mesher_;
    const Tasks::Handle handle = Pool_->Post(
        [raw, heights, mesher, out] { Generators::BakeStructures(*raw, *heights, *mesher, *out); });
    Queue_.push_back({.Tile = next->Tile,
                      .Raw = std::move(raw),
                      .Heights = std::move(heights),
                      .Out = std::move(out),
                      .Handle = handle});
    ++Posted_;
    ++posted;
  }
  return posted;
}

size_t StructureBakes::Lands(Ground::GroundStack &stack, TilePieces &pieces, size_t most) {
  if (Pool_ == nullptr || stack.Vectors() == nullptr) { return 0; }
  size_t landed = 0;
  while (!Queue_.empty() && landed < most && Pool_->Done(Queue_.front().Handle)) {
    const Job &job = Queue_.front();
    const Generators::BakedTile &baked = *job.Out;
    const size_t triangles = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3u;
    stack.Footprints().Accept(job.Tile,
                              *stack.Vectors(),
                              {.Prints = baked.Prints,
                               .SeatSpreadM = baked.SeatSpreadM,
                               .AcrossM = baked.AcrossM,
                               .Triangles = triangles,
                               .OsmHeights = baked.OsmHeights,
                               .DefaultHeights = baked.DefaultHeights,
                               .Fronted = baked.Fronted});
    if (triangles > 0) { pieces.Hands(job.Tile, baked.Built, job.Raw->AnchorEcef); }
    Log::Info(LogTag::World,
              "buildings",
              {{"added", static_cast<int>(baked.Prints.size())},
               {"total", static_cast<int>(stack.Footprints().Footprints().size())},
               {"osmHeight", stack.Footprints().OsmHeights()},
               {"defaultHeight", stack.Footprints().DefaultHeights()},
               {"vertsMB", static_cast<double>(baked.Built.UsedBytes()) / kBytesPerMB},
               {"lumped", baked.Lumped},
               {"blocks", baked.Blocks},
               {"awayKm", job.Raw->AwayM / kMPerKm},
               {"queued", static_cast<int>(Queue_.size() - 1)}});
    Queue_.pop_front();
    ++Landed_;
    ++landed;
  }
  return landed;
}

void StructureBakes::Clear() {
  for (const Job &job : Queue_) {
    if (Pool_ != nullptr && job.Handle != Tasks::kNoTask) { Pool_->Wait(job.Handle); }
  }
  Queue_.clear();
}

} // namespace outshine
