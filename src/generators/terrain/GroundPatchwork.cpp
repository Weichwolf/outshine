#include "GroundPatchwork.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr long kBlockTiles = 4;
constexpr size_t kTilesPerLevel = kBlockTiles * kBlockTiles;

namespace Says {
constexpr auto Input = "patchwork requires a supported positive zoom, positive levels, grid >= 2 "
                       "and finite coordinates";
constexpr auto Empty = "patchwork selected no valid terrain tiles";
}

struct TileBlock {
  int Zoom = 0;
  long X = 0, Y = 0;
  long ContactX = 0, ContactY = 0;
  long Span = 1;
};

class Coverage {
public:
  void BeginLevel(const TileBlock &block) {
    if (!Empty_) {
      std::array<uint64_t, kTilesPerLevel> parent{};
      for (long row = 0; row < kBlockTiles; ++row) {
        for (long column = 0; column < kBlockTiles; ++column) {
          const long x = (X_ + column - 2 * block.X) / 2;
          const long y = (Y_ + row - 2 * block.Y) / 2;
          parent[static_cast<size_t>(y * kBlockTiles + x)] +=
              Cells_[static_cast<size_t>(row * kBlockTiles + column)];
        }
      }
      Cells_ = parent;
    }
    X_ = block.X;
    Y_ = block.Y;
  }

  [[nodiscard]] uint64_t CoveredCells(long x, long y) const { return Cells_[Index(x, y)]; }

  void Add(long x, long y, uint64_t cells) {
    Cells_[Index(x, y)] = cells;
    Empty_ = false;
  }

private:
  [[nodiscard]] size_t Index(long x, long y) const {
    return static_cast<size_t>((y - Y_) * kBlockTiles + x - X_);
  }

  std::array<uint64_t, kTilesPerLevel> Cells_{};
  long X_ = 0, Y_ = 0;
  bool Empty_ = true;
};

long BlockOrigin(double tileCoordinate) {
  return kBlockTiles / 2 * static_cast<long>(std::floor((std::floor(tileCoordinate) - 1) / 2));
}

[[nodiscard]] bool ValidCoverage(const Around &over) {
  return over.Zoom >= 1 && std::cmp_less(over.Zoom, kZoomLevels) && over.Levels >= 1 &&
         over.Grid >= 2 && std::isfinite(over.LatitudeDeg) && std::isfinite(over.LongitudeDeg);
}

TileBlock BlockAt(const Around &over, int level) {
  const int zoom = over.Zoom - level;
  const auto at = Ground::ToTileFracClamped(
      {.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, zoom);
  return {.Zoom = zoom,
          .X = BlockOrigin(at.X),
          .Y = BlockOrigin(at.Y),
          .ContactX = static_cast<long>(std::floor(at.X)),
          .ContactY = static_cast<long>(std::floor(at.Y)),
          .Span = 1L << static_cast<unsigned>(level)};
}

template <typename Visit> void ForEachBlockTile(const TileBlock &block, Visit visit) {
  for (long row = 0; row < kBlockTiles; ++row) {
    for (long column = 0; column < kBlockTiles; ++column) {
      const long x = block.X + column;
      const long y = block.Y + row;
      const bool contact = block.Span == 1 && x == block.ContactX && y == block.ContactY;
      visit(block.Zoom, x, y, contact);
    }
  }
}

bool RecordReply(TileMeshes::Reply reply, int zoom, Patchwork &out) {
  ++out.WantedAtZoom[zoom];
  switch (reply) {
    case TileMeshes::Reply::Ready: return true;
    case TileMeshes::Reply::Pending:
    case TileMeshes::Reply::Deferred:
      ++out.Pending;
      ++out.PendingAtZoom[zoom];
      return false;
    case TileMeshes::Reply::Absent:
    case TileMeshes::Reply::Undeclared: ++out.Absent; return false;
    case TileMeshes::Reply::Refused: ++out.Refused; return false;
  }
  return false;
}

bool RequestTile(TileMeshes &tiles, const Around &over, Data::TileId asked, Patchwork &out) {
  TileBuild built;
  const auto reply = [&] {
    switch (over.Access) {
      case TerrainTileAccess::Mesh: return tiles.Mesh(asked, over.Grid, &built);
      case TerrainTileAccess::Request: return tiles.Wants(asked, over.Grid);
      case TerrainTileAccess::Inspect: return tiles.Inspect(asked, over.Grid);
    }
    return TileMeshes::Reply::Refused;
  }();
  const bool metadataOnly = over.Access != TerrainTileAccess::Mesh;
  const bool available = RecordReply(reply, asked.Zoom, out);
  const bool ready = available && (metadataOnly || (built.Side >= 2 && !built.Nodes.empty()));
  ++out.Tiles;
  if (!metadataOnly) {
    if (!ready) { ++out.Bare; }
    if (ready) {
      out.Sheets.push_back({.Tile = asked,
                            .Nodes = std::move(built.Nodes),
                            .Side = built.Side,
                            .Postings = built.Postings});
    }
  }
  return ready;
}

void LayLevel(TileMeshes &tiles,
              const Around &over,
              int level,
              int lastLevel,
              Coverage &coverage,
              Patchwork &out) {
  const TileBlock block = BlockAt(over, level);
  coverage.BeginLevel(block);
  const uint64_t cells = static_cast<uint64_t>(block.Span) * static_cast<uint64_t>(block.Span);
  ForEachBlockTile(block, [&](int tileZoom, long x, long y, bool contact) {
    if (over.Access != TerrainTileAccess::Mesh && over.PlayableOnly && level != lastLevel &&
        !contact) {
      return;
    }
    const uint64_t covered = coverage.CoveredCells(x, y);
    if (covered == cells) {
      ++out.Skipped;
      return;
    }
    if (covered > 0) { ++out.Overlapped; }
    const long unwrappedX = x;
    const long unwrappedY = y;
    if (!Ground::WrapTile(tileZoom, &x, &y)) { return; }
    const Data::TileId asked{
        .Zoom = tileZoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
    const bool ready = RequestTile(tiles, over, asked, out);
    if (contact && !ready) { ++out.ContactPending; }
    if (ready) { coverage.Add(unwrappedX, unwrappedY, cells); }
  });
  out.ReachTiles = kBlockTiles * block.Span;
  out.CoarsestZoom = block.Zoom;
}

}

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (!ValidCoverage(over)) { return std::unexpected(Says::Input); }
  const int levels = std::min(over.Levels, over.Zoom);
  Patchwork out;
  if (over.Access == TerrainTileAccess::Mesh) {
    out.Sheets.reserve(kTilesPerLevel * static_cast<size_t>(levels));
  }
  Coverage coverage;
  for (int level = 0; level < levels; ++level) {
    LayLevel(tiles, over, level, levels - 1, coverage, out);
  }
  if (out.Tiles == 0) { return std::unexpected(Says::Empty); }
  return out;
}

std::expected<std::vector<Data::TileId>, std::string> PlanPatchworkTiles(const Around &over) {
  if (!ValidCoverage(over)) { return std::unexpected(Says::Input); }
  const int levels = std::min(over.Levels, over.Zoom);
  std::vector<Data::TileId> tiles;
  tiles.reserve(kTilesPerLevel * static_cast<size_t>(levels));
  for (int level = 0; level < levels; ++level) {
    ForEachBlockTile(BlockAt(over, level), [&](int zoom, long x, long y, bool) {
      if (!Ground::WrapTile(zoom, &x, &y)) { return; }
      tiles.push_back({.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
    });
  }
  const auto key = [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); };
  std::ranges::sort(tiles, {}, key);
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  return tiles;
}

std::expected<Patchwork, std::string> Patchworker::Lay(TileMeshes &tiles,
                                                       const Around &over) const {
  return LayPatchwork(tiles, over);
}

}
