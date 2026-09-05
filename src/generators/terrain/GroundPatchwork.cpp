#include "GroundPatchwork.h"

#include "GroundYield.h"
#include "math/Vec3.h"

#include <cmath>
#include <utility>
#include <span>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <string>

#include "TileGeodesy.h"

namespace outshine {

constexpr long kBlockTiles = 4;

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (over.Zoom <= 0) {
    return std::unexpected("a patchwork is laid at a declared zoom, and this one asks for " +
                           std::to_string(over.Zoom));
  }
  if (over.Levels < 1) {
    return std::unexpected("a cascade lays at least its finest level, and this one lays " +
                           std::to_string(over.Levels));
  }

  Patchwork out;

  const int levels = over.Levels < 1 ? 1 : over.Levels;
  const long widest = kBlockTiles * (1L << static_cast<uint32_t>(levels - 1));
  long maskX0 = 0;
  long maskY0 = 0;
  {
    const int coarsest = over.Zoom - (levels - 1) < 1 ? 1 : over.Zoom - (levels - 1);
    const long span = 1L << static_cast<uint32_t>(over.Zoom - coarsest);
    const Ground::TileFrac at = Ground::ToTileFracClamped(
        Ground::Geo{.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, coarsest);
    maskX0 = 2 *
             static_cast<long>(std::floor(
                 (static_cast<double>(static_cast<long>(std::floor(at.X))) - 1.0) / 2.0)) *
             span;
    maskY0 = 2 *
             static_cast<long>(std::floor(
                 (static_cast<double>(static_cast<long>(std::floor(at.Y))) - 1.0) / 2.0)) *
             span;
  }
  std::vector<uint8_t> covering(static_cast<size_t>(widest) * static_cast<size_t>(widest), 0u);
  const auto marked = [&covering, widest, maskX0, maskY0](long fx, long fy) -> uint8_t * {
    const long ix = fx - maskX0;
    const long iy = fy - maskY0;
    if (ix < 0 || iy < 0 || ix >= widest || iy >= widest) { return nullptr; }
    return &covering[static_cast<size_t>(iy) * static_cast<size_t>(widest) +
                     static_cast<size_t>(ix)];
  };
  for (int level = 0; level < levels; ++level) {
    const int zoom = over.Zoom - level;
    if (zoom < 1) { break; }
    const long span = 1L << static_cast<uint32_t>(level);
    const Ground::TileFrac at = Ground::ToTileFracClamped(
        Ground::Geo{.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, zoom);
    const long originX =
        2 * static_cast<long>(
                std::floor((static_cast<double>(static_cast<long>(std::floor(at.X))) - 1.0) / 2.0));
    const long originY =
        2 * static_cast<long>(
                std::floor((static_cast<double>(static_cast<long>(std::floor(at.Y))) - 1.0) / 2.0));
    std::vector<std::pair<long, long>> standing;
    for (long row = 0; row < kBlockTiles; ++row) {
      for (long column = 0; column < kBlockTiles; ++column) {
        long x = originX + column;
        const long y = originY + row;
        const long heldX0 = x * span;
        const long heldX1 = heldX0 + span - 1;
        const long heldY0 = y * span;
        const long heldY1 = heldY0 + span - 1;
        bool covered = true;
        bool touches = false;
        for (long fy = heldY0; fy <= heldY1; ++fy) {
          for (long fx = heldX0; fx <= heldX1; ++fx) {
            const uint8_t *const cell = marked(fx, fy);
            if (cell != nullptr && *cell != 0u) {
              touches = true;
            } else {
              covered = false;
            }
          }
        }
        if (covered) {
          ++out.Skipped;
          continue;
        }
        if (touches) { ++out.Overlapped; }
        if (!Ground::WrapTile(zoom, &x, &y)) { continue; }
        const Data::TileId asked = {
            .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
        TileBuild built;
        const TileMeshes::Reply said =
            over.Asking ? tiles.Wants(asked, over.Grid) : tiles.Mesh(asked, over.Grid, &built);
        bool ofTheGround = true;
        if (zoom >= 0 && std::cmp_less(zoom, kZoomLevels)) { ++out.WantedAtZoom[zoom]; }
        if (said == TileMeshes::Reply::Pending) {
          ++out.Pending;
          if (zoom >= 0 && std::cmp_less(zoom, kZoomLevels)) { ++out.PendingAtZoom[zoom]; }
          ofTheGround = false;
        } else if (said == TileMeshes::Reply::Absent || said == TileMeshes::Reply::Undeclared) {
          ++out.Absent;
          ofTheGround = false;
        } else if (said == TileMeshes::Reply::Refused) {
          ++out.Refused;
          ofTheGround = false;
        }
        if (ofTheGround && (built.Side < 2 || built.Nodes.empty())) {
          ofTheGround = false;
          if (!over.Asking) {}
        }
        if (!ofTheGround && !over.Asking) { ++out.Bare; }
        if (over.Asking) {
          ++out.Tiles;
          if (ofTheGround) { standing.emplace_back(heldX0, heldY0); }
          continue;
        }
        out.Sheets.push_back({.Tile = asked,
                              .Nodes = std::move(built.Nodes),
                              .Side = built.Side,
                              .Postings = built.Postings});
        ++out.Tiles;
        if (ofTheGround) { standing.emplace_back(heldX0, heldY0); }
      }
    }
    for (const auto &one : standing) {
      for (long fy = one.second; fy < one.second + span; ++fy) {
        for (long fx = one.first; fx < one.first + span; ++fx) {
          uint8_t *const cell = marked(fx, fy);
          if (cell != nullptr) { *cell = 1u; }
        }
      }
    }
    standing.clear();
    out.ReachTiles = kBlockTiles * span;
    out.CoarsestZoom = zoom;
  }

  if (!over.Asking) {}
  if (out.Tiles == 0) {
    return std::unexpected("no tile of the " + std::to_string(levels) + "-level cascade around " +
                           std::to_string(over.LatitudeDeg) + ", " +
                           std::to_string(over.LongitudeDeg) + " meshed -- " +
                           std::to_string(out.Pending) + " pending, " + std::to_string(out.Absent) +
                           " absent, " + std::to_string(out.Refused) + " refused");
  }
  return out;
}

std::expected<Patchwork, std::string> Patchworker::Lay(TileMeshes &tiles,
                                                       const Around &over) const {
  return LayPatchwork(tiles, over);
}

} // namespace outshine
