#include "TerrainPathPreparation.h"
#include "GroundPatchwork.h"
#include "TerrainSourceCoverage.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

namespace outshine {
namespace {
struct TileLess {
  bool operator()(Data::TileId a, Data::TileId b) const noexcept {
    return std::tie(a.Zoom, a.X, a.Y) < std::tie(b.Zoom, b.X, b.Y);
  }
};

using Tiles = std::set<Data::TileId, TileLess>;

struct PathTiles {
  Tiles Meshes;
  Tiles Samples;
  Tiles Vectors;
};

constexpr double kMostAwaitS = 0.01;
using Result = std::expected<void, std::string>;
constexpr auto kBudgetError = "view data path exceeds its unique tile budget";

Result Add(Tiles &tiles, Data::TileId tile) {
  if (tiles.contains(tile)) { return {}; }
  if (tiles.size() == TerrainPathPlan::MaximumTiles) { return std::unexpected(kBudgetError); }
  tiles.insert(tile);
  return {};
}

Result Add(Tiles &tiles, std::span<const Data::TileId> more) {
  for (const auto tile : more) {
    if (const auto added = Add(tiles, tile); !added) { return added; }
  }
  return {};
}

Result AddWindow(Tiles &tiles, LongitudeLatitude at, int zoom, int radius) {
  const auto window =
      Ground::OsmField::SourceWindow(at, zoom, radius, TerrainPathPlan::MaximumTiles);
  if (!window) { return std::unexpected(std::string(window.error())); }
  for (int64_t y = window->MinY; y <= window->MaxY; ++y) {
    for (int64_t x = window->MinX; x <= window->MaxX; ++x) {
      if (const auto added = Add(
              tiles, {.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
          !added) {
        return added;
      }
    }
  }
  return {};
}

Result AddPoint(PathTiles &tiles,
                const Around &point,
                const Ground::GroundStream &ground,
                TerrainPathSources sources) {
  const auto planned = PlanPatchworkTiles(point);
  if (!planned) { return std::unexpected(planned.error()); }
  if (const auto added = Add(tiles.Meshes, *planned); !added) { return added; }
  const LongitudeLatitude at{.LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg};
  const auto coverage = ground.SamplingCoverage(at);
  if (!coverage) { return std::unexpected("view data path has invalid sampling coverage"); }
  if (const auto added = Add(tiles.Samples, coverage->Fine); !added) { return added; }
  if (coverage->Coarse) {
    for (const auto tile : *planned) {
      if (tile.Zoom < coverage->Coarse->Zoom) { continue; }
      const auto drop = static_cast<uint32_t>(tile.Zoom - coverage->Coarse->Zoom);
      if (const auto added =
              Add(tiles.Samples,
                  {.Zoom = coverage->Coarse->Zoom, .X = tile.X >> drop, .Y = tile.Y >> drop});
          !added) {
        return added;
      }
    }
  }
  if (sources.VectorZoom >= 0) {
    if (const auto added = AddWindow(tiles.Vectors, at, sources.VectorZoom, Ground::kVectorRing);
        !added) {
      return added;
    }
  }
  for (const auto window : sources.Classification) {
    if (const auto added = AddWindow(tiles.Vectors, at, window.Zoom, window.Ring); !added) {
      return added;
    }
  }
  return {};
}

Result Await(Ground::TilePool &pool,
             std::chrono::steady_clock::time_point deadline,
             Data::TileId tile,
             std::string_view kind) {
  const double remaining =
      std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count();
  if (remaining <= 0.0) {
    return std::unexpected(std::format(
        "view data preparation timed out at {}/{}/{}/{}", kind, tile.Zoom, tile.X, tile.Y));
  }
  static_cast<void>(pool.AwaitLanding(std::min(remaining, kMostAwaitS)));
  return {};
}

template <class Poll>
std::expected<size_t, std::string>
PollBatch(std::span<const Data::TileId> batch, std::span<bool> settled, Poll &poll) {
  size_t progress = 0;
  for (size_t index = 0; index < batch.size(); ++index) {
    if (settled[index]) { continue; }
    const auto result = poll(batch[index]);
    if (!result) { return std::unexpected(result.error()); }
    if (*result) {
      settled[index] = true;
      ++progress;
    }
  }
  return progress;
}

template <class Poll>
Result SettleTiles(std::span<const Data::TileId> tiles,
                   Ground::TilePool &pool,
                   std::chrono::steady_clock::time_point deadline,
                   std::string_view kind,
                   size_t batchMost,
                   Poll poll) {
  constexpr size_t kBatchTiles = 64;
  for (size_t first = 0; first < tiles.size(); first += batchMost) {
    const auto batch = tiles.subspan(first, std::min(batchMost, tiles.size() - first));
    if (first != 0 && std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected(std::format("view data preparation timed out at {}/{}/{}/{}",
                                         kind,
                                         batch.front().Zoom,
                                         batch.front().X,
                                         batch.front().Y));
    }
    std::array<bool, kBatchTiles> settled{};
    size_t remaining = batch.size();
    while (remaining != 0) {
      const auto progress = PollBatch(batch, std::span(settled).first(batch.size()), poll);
      if (!progress) { return std::unexpected(progress.error()); }
      remaining -= *progress;
      if (remaining == 0) { break; }
      if (const auto waited =
              Await(pool,
                    deadline,
                    batch[static_cast<size_t>(std::ranges::find(settled, false) - settled.begin())],
                    kind);
          !waited) {
        return waited;
      }
    }
  }
  return {};
}

std::string Refusal(Data::TileId tile,
                    std::string_view kind,
                    const std::optional<Data::FetchFailure> &failure) {
  const std::string context =
      std::format("view data preparation refused {}/{}/{}/{}", kind, tile.Zoom, tile.X, tile.Y);
  if (!failure) { return context; }
  return std::format("{}: {} at {}/{}; served={}; source={}; revision={}; key={}",
                     context,
                     Data::Name(failure->Reason),
                     Data::Name(failure->Kind),
                     failure->Requested.Text(),
                     failure->Served ? failure->Served->Text() : "unknown",
                     failure->SourceId,
                     failure->SourceRevision,
                     failure->SourceKey);
}

std::expected<bool, std::string>
PollVector(Data::TileId tile, const Ground::GroundStack &stack, Ground::OsmField &vectors) {
  Ground::TilePool::Landing landed;
  const auto status =
      stack.Pool().Bytes({Data::DataKind::VectorMap, Data::Address::At(tile)}, &landed);
  if (status == Ground::TilePool::Reply::Absent || status == Ground::TilePool::Reply::Undeclared) {
    return true;
  }
  if (status == Ground::TilePool::Reply::Refused) {
    return std::unexpected(Refusal(tile, "vector", landed.Failure));
  }
  if (status != Ground::TilePool::Reply::Ready) { return false; }
  if (tile.Zoom == vectors.Zoom()) {
    const auto parsed =
        vectors.Accept(static_cast<int>(tile.X), static_cast<int>(tile.Y), landed.Bytes);
    if (!parsed) {
      return std::unexpected(std::format("view data decode failed at vector/{}/{}/{}: {}",
                                         tile.Zoom,
                                         tile.X,
                                         tile.Y,
                                         parsed.error()));
    }
  }
  return true;
}

std::expected<bool, std::string> PollField(Data::TileId tile, const Ground::GroundStack &stack) {
  std::shared_ptr<const Ground::TerrainField> field;
  std::optional<Data::FetchFailure> failure;
  const auto status = stack.Ground().PollStitchedField(tile, field, &failure);
  if (status == Ground::TilePool::Reply::Refused) {
    return std::unexpected(Refusal(tile, "elevation", failure));
  }
  if (status == Ground::TilePool::Reply::Ready && !field) {
    return std::unexpected("view data preparation received a ready field without data");
  }
  return status != Ground::TilePool::Reply::Pending && status != Ground::TilePool::Reply::Deferred;
}

}

std::expected<TerrainPathPlan, std::string> PlanTerrainPath(std::span<const Around> path,
                                                            const Ground::GroundStream &ground,
                                                            TerrainPathSources sources) {
  if (path.empty() || path.size() > TerrainPathPlan::MaximumPoints) {
    return std::unexpected("view data path has an invalid point count");
  }
  PathTiles tiles;
  const int finest = path.front().Zoom;
  for (const auto &point : path) {
    if (point.Zoom != finest) { return std::unexpected("view data path changes its source grid"); }
    const auto added = AddPoint(tiles, point, ground, sources);
    if (!added) { return std::unexpected(added.error()); }
  }
  const std::vector<Data::TileId> meshTiles(tiles.Meshes.begin(), tiles.Meshes.end());
  const auto fields = PlanTerrainSourceTiles(meshTiles,
                                             {.FinestZoom = finest,
                                              .GroundZoom = ground.BlockZoom(),
                                              .AdditionalTiles = sources.RoadTiles});
  if (!fields) { return std::unexpected(fields.error()); }
  if (const auto added = Add(tiles.Samples, *fields); !added) {
    return std::unexpected(added.error());
  }
  return TerrainPathPlan{.Fields = {tiles.Samples.begin(), tiles.Samples.end()},
                         .Vectors = {tiles.Vectors.begin(), tiles.Vectors.end()}};
}

std::expected<void, std::string>
PrepareTerrainPath(TerrainPathPlan plan,
                   const Ground::GroundStack &stack,
                   std::chrono::steady_clock::time_point deadline) {
  auto vectors = stack.CreateVectorField();
  if (const auto read =
          SettleTiles(plan.Vectors,
                      stack.Pool(),
                      deadline,
                      "vector",
                      64,
                      [&](Data::TileId tile) { return PollVector(tile, stack, *vectors); });
      !read) {
    return read;
  }
  const auto heights = PlanTerrainSourceTiles(
      {},
      {.FinestZoom = stack.FinestZoomOf(Data::DataKind::Elevation),
       .GroundZoom = stack.Ground().BlockZoom(),
       .Vectors = stack.HasDeclaredVectors() ? stack.Vectors() : vectors.get(),
       .AdditionalTiles = {}});
  if (!heights) { return std::unexpected(heights.error()); }
  Tiles fields(plan.Fields.begin(), plan.Fields.end());
  if (const auto added = Add(fields, *heights); !added) { return added; }
  const std::vector<Data::TileId> allFields(fields.begin(), fields.end());
  return SettleTiles(allFields, stack.Pool(), deadline, "elevation", 4, [&](Data::TileId tile) {
    return PollField(tile, stack);
  });
}
}
