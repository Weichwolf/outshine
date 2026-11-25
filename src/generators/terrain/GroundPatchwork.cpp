#include "GroundPatchwork.h"
#include "Box.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include "Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr long kBlockTiles = 4;
constexpr size_t kTilesPerLevel = kBlockTiles * kBlockTiles;

auto TileAddress(Data::TileId tile) {
  return std::tuple(tile.Zoom, tile.X, tile.Y);
}

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

Ground::GeoBounds FinestBlockBounds(const TileBlock &block) {
  const double tiles = std::ldexp(1.0, block.Zoom);
  const auto northwest = Ground::TileFracToGeo(
      {.X = static_cast<double>(block.X), .Y = static_cast<double>(std::max(0L, block.Y))},
      block.Zoom);
  const auto southeast =
      Ground::TileFracToGeo({.X = static_cast<double>(block.X + kBlockTiles),
                             .Y = std::min(tiles, static_cast<double>(block.Y + kBlockTiles))},
                            block.Zoom);
  return {.MinLonDeg = northwest.LongitudeDeg,
          .MinLatDeg = southeast.LatitudeDeg,
          .MaxLonDeg = southeast.LongitudeDeg,
          .MaxLatDeg = northwest.LatitudeDeg};
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

std::expected<Patchwork, std::string> LaySweptPatchwork(TileMeshes &tiles, const Around &over) {
  const auto &patches = over.Sweep->Patches;
  if (patches.empty()) { return std::unexpected(Says::Empty); }
  Patchwork out;
  out.CoarsestZoom = patches.back().Zoom;
  if (over.Access == TerrainTileAccess::Mesh) { out.Sheets.reserve(patches.size()); }
  for (const auto tile : patches) {
    if (!RequestTile(tiles, over, tile, out) && tile.Zoom == over.Zoom) { ++out.ContactPending; }
  }
  out.ReachTiles = kBlockTiles * (1L << static_cast<unsigned>(over.Zoom - out.CoarsestZoom));
  return out;
}

}

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (!ValidCoverage(over)) { return std::unexpected(Says::Input); }
  if (over.Sweep) { return LaySweptPatchwork(tiles, over); }
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
  if (over.Sweep) { return over.Sweep->Requested; }
  const int levels = std::min(over.Levels, over.Zoom);
  std::vector<Data::TileId> tiles;
  tiles.reserve(kTilesPerLevel * static_cast<size_t>(levels));
  for (int level = 0; level < levels; ++level) {
    ForEachBlockTile(BlockAt(over, level), [&](int zoom, long x, long y, bool) {
      if (!Ground::WrapTile(zoom, &x, &y)) { return; }
      tiles.push_back({.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
    });
  }
  std::ranges::sort(tiles, {}, TileAddress);
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  return tiles;
}

std::vector<Data::TileId> RemoveCoveredPatchworkTiles(std::span<const Data::TileId> requested) {
  if (requested.empty()) { return {}; }
  std::vector<Data::TileId> selected;
  selected.reserve(requested.size());
  auto until = requested.end();
  std::map<std::pair<uint32_t, uint32_t>, unsigned> fine;
  for (int zoom = requested.back().Zoom; zoom > 0; --zoom) {
    const auto first =
        std::ranges::lower_bound(requested.begin(), until, zoom, {}, &Data::TileId::Zoom);
    for (auto at = first; at != until; ++at) {
      const auto count = fine.find({at->X, at->Y});
      if (count == fine.end() || count->second < 4) { selected.push_back(*at); }
      fine[{at->X, at->Y}] = 4;
    }
    decltype(fine) parents;
    for (const auto &[position, count] : fine) {
      if (count == 4) { ++parents[{position.first >> 1u, position.second >> 1u}]; }
    }
    fine = std::move(parents);
    until = first;
  }
  return selected;
}

bool SweptPatchworkCovers(const TerrainSweep &sweep, const Around &request, const Vec3 &eyeM) {
  if (!sweep.Eyes.Holds(eyeM) ||
      (sweep.EyePath && !(sweep.EyePath->DistanceTo(eyeM) <= sweep.EyePath->RadiusM))) {
    return false;
  }
  const auto wanted = PlanPatchworkTiles(request);
  return wanted && std::ranges::includes(sweep.Requested, *wanted, {}, TileAddress, TileAddress);
}

std::optional<Ground::GeoBounds> PatchworkFocusWindow(const Around &coverage) {
  if (!ValidCoverage(coverage) || coverage.Sweep) { return std::nullopt; }
  const auto block = BlockAt(coverage, 0);
  auto bounds = FinestBlockBounds(block);
  if (block.Y <= 0) { bounds.MaxLatDeg = kDegPerHalfTurn * 0.5; }
  if (static_cast<double>(block.Y + kBlockTiles) >= std::ldexp(1.0, block.Zoom)) {
    bounds.MinLatDeg = -kDegPerHalfTurn * 0.5;
  }
  return bounds;
}

bool PatchworkIncludesContactTile(const Around &coverage, const Around &request) noexcept {
  if (!ValidCoverage(coverage) || !ValidCoverage(request) || coverage.Zoom != request.Zoom ||
      coverage.Levels != request.Levels || coverage.Grid != request.Grid) {
    return false;
  }
  const TileBlock covered = BlockAt(coverage, 0);
  const TileBlock wanted = BlockAt(request, 0);
  long column = wanted.ContactX - covered.X;
  if (!Ground::WrapTile(coverage.Zoom, &column, &wanted.ContactY)) { return false; }
  const long row = wanted.ContactY - covered.Y;
  return column < kBlockTiles && row >= 0 && row < kBlockTiles;
}

namespace {
bool RetainsCoarseBlocks(const Around &coverage, const Around &request) noexcept {
  if (!PatchworkIncludesContactTile(coverage, request)) { return false; }
  const int levels = std::min(coverage.Levels, coverage.Zoom);
  for (int level = 1; level < levels; ++level) {
    const TileBlock held = BlockAt(coverage, level);
    const TileBlock wanted = BlockAt(request, level);
    if (held.X != wanted.X || held.Y != wanted.Y) { return false; }
  }
  return true;
}

constexpr double kEdgeRoundoffM = 256 * std::numeric_limits<double>::epsilon() * kWgs84A;
enum class EdgeBound { Lower, Upper };

bool InsideMeridian(std::span<const Vec3> corners, double degrees, EdgeBound bound) noexcept {
  const double inward = bound == EdgeBound::Lower ? 1 : -1;
  const double angle = degrees * kDeg2Rad;
  const Vec3 normal{{-inward * std::sin(angle), inward * std::cos(angle), 0}};
  return std::ranges::all_of(
      corners, [&](const Vec3 &corner) { return Dot(normal, corner) > kEdgeRoundoffM; });
}

bool InsideParallel(std::span<const Vec3> corners,
                    const Box &rotated,
                    double degrees,
                    EdgeBound bound) noexcept {
  if (!(rotated.Min[0] > 0)) { return false; }
  const double inward = bound == EdgeBound::Lower ? 1 : -1;
  const double angle = degrees * kDeg2Rad;
  const double vertical = inward * std::cos(angle);
  const double sine = std::sin(angle);
  const double radial = -inward * sine;
  const double primeVertical = kWgs84A / std::sqrt(1 - kWgs84E2 * sine * sine);
  const double offset = kWgs84E2 * primeVertical * sine * vertical;
  const double outerEast = std::max(std::abs(rotated.Min[1]), std::abs(rotated.Max[1]));
  const double radialSlack = outerEast * outerEast / (2 * rotated.Min[0]);
  const double margin = kEdgeRoundoffM + std::max(0.0, -radial * radialSlack);
  return std::ranges::all_of(corners, [&](const Vec3 &corner) {
    return radial * corner[0] + vertical * corner[2] + offset > margin;
  });
}
}

bool PatchworkCoversFocus(const Around &coverage, const Around &request, double radiusM) noexcept {
  if (!std::isfinite(radiusM) || radiusM <= 0.0 || !RetainsCoarseBlocks(coverage, request)) {
    return false;
  }
  const auto block = BlockAt(coverage, 0);
  const auto bounds = FinestBlockBounds(block);
  constexpr double minimumRadiusM = kWgs84A * (1.0 - kWgs84E2);
  const double radiusRad = radiusM / minimumRadiusM;
  const double latSpan = radiusRad * kRad2Deg;
  const double latitude = request.LatitudeDeg;
  if (latitude - latSpan <= bounds.MinLatDeg || latitude + latSpan >= bounds.MaxLatDeg) {
    return false;
  }
  if (std::ldexp(1.0, coverage.Zoom) <= kBlockTiles) { return true; }
  const double lonSpan = radiusRad * kRad2Deg / std::cos((std::abs(latitude) + latSpan) * kDeg2Rad);
  const double offset = request.LongitudeDeg - bounds.MinLonDeg;
  const double eastward = offset - kDegPerTurn * std::floor(offset / kDegPerTurn);
  const double width = bounds.MaxLonDeg - bounds.MinLonDeg;
  return lonSpan < eastward && eastward + lonSpan < width;
}

bool PatchworkCoversSamples(const Around &coverage,
                            const Around &request,
                            const TangentFrame &frame,
                            const Box &localBounds) noexcept {
  if (localBounds.Empty() || !RetainsCoarseBlocks(coverage, request)) { return false; }
  const auto window = FinestBlockBounds(BlockAt(coverage, 0));
  const double width = window.MaxLonDeg - window.MinLonDeg;
  std::array<Vec3, 8> corners;
  const auto &east = frame.EastEcef();
  const Vec3 radial{{east[1], -east[0], 0}};
  const double origin = frame.Anchor().LongitudeDeg;
  Box rotated;
  for (unsigned i = 0; i < corners.size(); ++i) {
    const auto point = localBounds.Corner(i);
    const auto corner =
        frame.ToEcefPosition({.EastM = point[0], .NorthM = point[1], .UpM = point[2]});
    corners[i] = {{Dot(radial, corner), Dot(east, corner), corner[2]}};
    rotated.Cover(corners[i]);
  }
  const bool betweenMeridians =
      width >= kDegPerTurn ||
      (width <= kDegPerHalfTurn &&
       InsideMeridian(corners, window.MinLonDeg - origin, EdgeBound::Lower) &&
       InsideMeridian(corners, window.MaxLonDeg - origin, EdgeBound::Upper));
  return betweenMeridians && InsideParallel(corners, rotated, window.MinLatDeg, EdgeBound::Lower) &&
         InsideParallel(corners, rotated, window.MaxLatDeg, EdgeBound::Upper);
}

std::expected<Patchwork, std::string> Patchworker::Lay(TileMeshes &tiles,
                                                       const Around &over) const {
  return LayPatchwork(tiles, over);
}

}
