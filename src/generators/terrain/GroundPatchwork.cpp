#include "GroundPatchwork.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>

namespace outshine {
namespace {

constexpr long kBlockTiles = 4;
constexpr size_t kTilesPerLevel = kBlockTiles * kBlockTiles;
constexpr size_t kMaxCascadeTiles = kTilesPerLevel * (kZoomLevels - 1);

namespace Says {
constexpr auto Input = "patchwork requires a supported positive zoom, positive levels, grid >= 2 "
                       "and finite coordinates";
constexpr auto Empty = "patchwork selected no valid terrain tiles";
}

struct TileRegion {
  long X = 0;
  long Y = 0;
  long Span = 1;

  [[nodiscard]] bool Contains(const TileRegion &other) const {
    return other.X >= X && other.Y >= Y && other.X + other.Span <= X + Span &&
           other.Y + other.Span <= Y + Span;
  }

  [[nodiscard]] uint64_t Cells() const {
    return static_cast<uint64_t>(Span) * static_cast<uint64_t>(Span);
  }
};

class Coverage {
public:
  [[nodiscard]] uint64_t CoveredCells(const TileRegion &region) const {
    uint64_t cells = 0;
    for (const auto &held : std::span(Regions_).first(Count_)) {
      if (region.Contains(held)) { cells += held.Cells(); }
    }
    return cells;
  }

  void Add(const TileRegion &region) {
    const auto active = std::span(Regions_).first(Count_);
    const auto retained = std::ranges::remove_if(
        active, [&](const TileRegion &held) { return region.Contains(held); });
    Count_ = static_cast<size_t>(retained.begin() - active.begin());
    assert(Count_ < Regions_.size());
    Regions_[Count_++] = region;
  }

private:
  std::array<TileRegion, kMaxCascadeTiles> Regions_{};
  size_t Count_ = 0;
};

long BlockOrigin(double tileCoordinate) {
  return kBlockTiles / 2 * static_cast<long>(std::floor((std::floor(tileCoordinate) - 1) / 2));
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
  const auto reply =
      over.Asking ? tiles.Wants(asked, over.Grid) : tiles.Mesh(asked, over.Grid, &built);
  const bool available = RecordReply(reply, asked.Zoom, out);
  const bool ready = available && (over.Asking || (built.Side >= 2 && !built.Nodes.empty()));
  ++out.Tiles;
  if (!over.Asking) {
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
  const int zoom = over.Zoom - level;
  const long span = 1L << static_cast<unsigned>(level);
  const auto at = Ground::ToTileFracClamped(
      {.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, zoom);
  const long originX = BlockOrigin(at.X);
  const long originY = BlockOrigin(at.Y);
  std::array<TileRegion, kTilesPerLevel> standing{};
  size_t count = 0;
  for (long row = 0; row < kBlockTiles; ++row) {
    for (long column = 0; column < kBlockTiles; ++column) {
      long x = originX + column;
      const long y = originY + row;
      const bool contact = level == 0 && x == static_cast<long>(std::floor(at.X)) &&
                           y == static_cast<long>(std::floor(at.Y));
      if (over.Asking && over.PlayableOnly && level != lastLevel && !contact) { continue; }
      const TileRegion region{.X = x * span, .Y = y * span, .Span = span};
      const uint64_t covered = coverage.CoveredCells(region);
      if (covered == region.Cells()) {
        ++out.Skipped;
        continue;
      }
      if (covered > 0) { ++out.Overlapped; }
      if (!Ground::WrapTile(zoom, &x, &y)) { continue; }
      const Data::TileId asked{
          .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
      const bool ready = RequestTile(tiles, over, asked, out);
      if (contact && !ready) { ++out.ContactPending; }
      if (ready) { standing[count++] = region; }
    }
  }
  for (const auto &region : std::span(standing).first(count)) { coverage.Add(region); }
  out.ReachTiles = kBlockTiles * span;
  out.CoarsestZoom = zoom;
}

}

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (over.Zoom < 1 || std::cmp_greater_equal(over.Zoom, kZoomLevels) || over.Levels < 1 ||
      over.Grid < 2 || !std::isfinite(over.LatitudeDeg) || !std::isfinite(over.LongitudeDeg)) {
    return std::unexpected(Says::Input);
  }
  const int levels = std::min(over.Levels, over.Zoom);
  Patchwork out;
  if (!over.Asking) { out.Sheets.reserve(kTilesPerLevel * static_cast<size_t>(levels)); }
  Coverage coverage;
  for (int level = 0; level < levels; ++level) {
    LayLevel(tiles, over, level, levels - 1, coverage, out);
  }
  if (out.Tiles == 0) { return std::unexpected(Says::Empty); }
  return out;
}

std::expected<Patchwork, std::string> Patchworker::Lay(TileMeshes &tiles,
                                                       const Around &over) const {
  return LayPatchwork(tiles, over);
}

}
