#include "TerrainPathPreparation.h"
#include "GroundPatchwork.h"
#include "TerrainSourceCoverage.h"
#include "PreparedTerrainAssets.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <map>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <iterator>
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
constexpr size_t kVectorRequestsMost = 64;
constexpr size_t kElevationRequestsMost = 4;
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
  const auto window = ::outshine::Generators::Osm::OsmField::SourceWindow(
      at, zoom, radius, TerrainPathPlan::MaximumTiles);
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
             Ground::TilePool::LandingCursor &cursor,
             std::chrono::steady_clock::time_point deadline,
             Data::TileId tile,
             std::string_view kind) {
  const double remaining =
      std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count();
  if (remaining <= 0.0) {
    return std::unexpected(std::format(
        "view data preparation timed out at {}/{}/{}/{}", kind, tile.Zoom, tile.X, tile.Y));
  }
  static_cast<void>(pool.AwaitLanding(std::min(remaining, kMostAwaitS), cursor));
  return {};
}

class TileWindow {
public:
  TileWindow(std::span<const Data::TileId> tiles, size_t width) : Tiles_(tiles), Width_(width) {}

  [[nodiscard]] bool Done() const noexcept { return Completed_ == Tiles_.size(); }

  [[nodiscard]] Data::TileId Next() const noexcept {
    for (const auto &tile : Active_) {
      if (tile) { return *tile; }
    }
    return Tiles_[Admitted_];
  }

  template <class Poll> [[nodiscard]] std::expected<bool, std::string> Advance(Poll &poll) {
    bool madeProgress = false;
    for (auto &tile : std::span(Active_).first(Width_)) {
      if (!tile && Admitted_ < Tiles_.size()) { tile = Tiles_[Admitted_++]; }
      if (!tile) { continue; }
      const auto ready = poll(*tile);
      if (!ready) { return std::unexpected(ready.error()); }
      if (*ready) {
        tile.reset();
        ++Completed_;
        madeProgress = true;
      }
    }
    return madeProgress;
  }

private:
  std::span<const Data::TileId> Tiles_;
  size_t Width_;
  size_t Admitted_ = 0;
  size_t Completed_ = 0;
  std::array<std::optional<Data::TileId>, kVectorRequestsMost> Active_{};
};

struct PathSourceTiles {
  std::span<const Data::TileId> Vectors;
  std::span<const Data::TileId> Heights;
};

template <class PollVector, class PollHeight>
Result SettleSources(PathSourceTiles tiles,
                     Ground::TilePool &pool,
                     std::chrono::steady_clock::time_point deadline,
                     PollVector &pollVector,
                     PollHeight &pollHeight) {
  TileWindow vectorWindow(tiles.Vectors, kVectorRequestsMost);
  TileWindow heightWindow(tiles.Heights, kElevationRequestsMost);
  Ground::TilePool::LandingCursor cursor;
  while (!vectorWindow.Done() || !heightWindow.Done()) {
    const auto vectorProgress = vectorWindow.Advance(pollVector);
    if (!vectorProgress) { return std::unexpected(vectorProgress.error()); }
    const auto heightProgress = heightWindow.Advance(pollHeight);
    if (!heightProgress) { return std::unexpected(heightProgress.error()); }
    if (vectorWindow.Done() && heightWindow.Done()) { break; }
    const bool vectorPending = !vectorWindow.Done();
    const auto now = std::chrono::steady_clock::now();
    if ((*vectorProgress || *heightProgress) && now < deadline) { continue; }
    const auto tile = vectorPending ? vectorWindow.Next() : heightWindow.Next();
    if (const auto waited =
            Await(pool, cursor, deadline, tile, vectorPending ? "vector" : "elevation");
        !waited) {
      return waited;
    }
  }
  return {};
}

std::string Refusal(Data::TileId tile,
                    std::string_view kind,
                    const std::optional<Data::SourceFailure> &failure) {
  std::string context =
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

using VectorField = Generators::Osm::OsmField;
using TilePresence = VectorField::TilePresence;

std::expected<TilePresence, std::string>
PrefetchVector(Data::TileId tile, const Ground::SurfacePreparation &stack, VectorField &vectors) {
  const auto loaded = vectors.PrefetchTile(
      stack.Pool(), {.X = static_cast<int>(tile.X), .Y = static_cast<int>(tile.Y)});
  if (!loaded) {
    return std::unexpected(std::format("view data preparation failed at vector/{}/{}/{}: {}",
                                       tile.Zoom,
                                       tile.X,
                                       tile.Y,
                                       loaded.error()));
  }
  return *loaded;
}

TilePresence PublishedVectorTile(const VectorField *field, Data::TileId tile) {
  if (field == nullptr || field->Zoom() != tile.Zoom) { return TilePresence::Pending; }
  const auto x = static_cast<int>(tile.X);
  const auto y = static_cast<int>(tile.Y);
  if (!field->Settled(x, y)) { return TilePresence::Pending; }
  if (field->TileIndex(x, y) >= 0) { return TilePresence::Present; }
  return field->PendingTiles() == 0 ? TilePresence::Absent : TilePresence::Pending;
}

std::expected<bool, std::string> PrepareVectorCoverage(Data::TileId tile,
                                                       const Ground::SurfacePreparation &stack,
                                                       VectorField &vectors,
                                                       std::vector<Data::TileId> &coverage) {
  const auto published = PublishedVectorTile(stack.Vectors(), tile);
  const auto available = published == TilePresence::Pending
                             ? PrefetchVector(tile, stack, vectors)
                             : std::expected<TilePresence, std::string>{published};
  if (!available) { return std::unexpected(available.error()); }
  if (*available == TilePresence::Present) { coverage.push_back(tile); }
  return *available != TilePresence::Pending;
}

std::expected<bool, std::string> PollField(Data::TileId tile,
                                           const Ground::SurfacePreparation &stack) {
  std::shared_ptr<const Ground::TerrainField> field;
  std::optional<Data::SourceFailure> failure;
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
  std::vector<Data::TileId> meshTiles(tiles.Meshes.begin(), tiles.Meshes.end());
  const auto fields = Generators::PlanTerrainSourceTiles(meshTiles,
                                                         {.FinestZoom = finest,
                                                          .GroundZoom = ground.BlockZoom(),
                                                          .AdditionalTiles = sources.RoadTiles});
  if (!fields) { return std::unexpected(fields.error()); }
  if (const auto added = Add(tiles.Samples, *fields); !added) {
    return std::unexpected(added.error());
  }
  return TerrainPathPlan{.Fields = {tiles.Samples.begin(), tiles.Samples.end()},
                         .Vectors = {tiles.Vectors.begin(), tiles.Vectors.end()},
                         .Meshes = std::move(meshTiles)};
}

std::expected<void, std::string>
PrepareTerrainPath(TerrainPathPlan plan,
                   const Ground::SurfacePreparation &stack,
                   std::chrono::steady_clock::time_point deadline) {
  if (plan.Fields.size() > TerrainPathPlan::MaximumTiles ||
      plan.Vectors.size() > TerrainPathPlan::MaximumTiles) {
    return std::unexpected(kBudgetError);
  }
  auto vectors = stack.CreateVectorField();
  std::map<int, std::unique_ptr<VectorField>> classification;
  std::vector<Data::TileId> vectorTiles;
  const auto pollVector = [&](Data::TileId tile) -> std::expected<bool, std::string> {
    if (tile.Zoom == vectors->Zoom()) {
      return PrepareVectorCoverage(tile, stack, *vectors, vectorTiles);
    }
    auto &field = classification[tile.Zoom];
    if (!field) { field = stack.Classes().CreateField(tile.Zoom); }
    if (!field) { return std::unexpected("unknown vector demand layout"); }
    const auto available = PrefetchVector(tile, stack, *field);
    if (!available) { return std::unexpected(available.error()); }
    return *available != TilePresence::Pending;
  };
  const auto &assets = stack.TerrainAssets();
  const auto shape = stack.Pool().Shaped();
  const auto pollHeight = [&](Data::TileId tile) -> std::expected<bool, std::string> {
    if (assets && assets->HasIndexedField(tile, shape)) { return true; }
    return PollField(tile, stack);
  };
  if (const auto read = SettleSources({.Vectors = plan.Vectors, .Heights = plan.Fields},
                                      stack.Pool(),
                                      deadline,
                                      pollVector,
                                      pollHeight);
      !read) {
    return read;
  }
  const auto heights = Generators::PlanTerrainSourceTiles(
      {},
      {.FinestZoom = stack.TerrainRenderZoom(),
       .GroundZoom = stack.Ground().BlockZoom(),
       .Vectors = stack.HasDeclaredVectors() ? stack.Vectors() : nullptr,
       .AdditionalTiles = {}},
      vectorTiles);
  if (!heights) { return std::unexpected(heights.error()); }
  std::vector<Data::TileId> additional;
  std::ranges::set_difference(*heights, plan.Fields, std::back_inserter(additional), TileLess{});
  if (additional.size() > TerrainPathPlan::MaximumTiles - plan.Fields.size()) {
    return std::unexpected(kBudgetError);
  }
  return SettleSources(
      {.Vectors = {}, .Heights = additional}, stack.Pool(), deadline, pollVector, pollHeight);
}
}
