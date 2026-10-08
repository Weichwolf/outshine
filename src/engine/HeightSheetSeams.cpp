#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>

#include "FlatMap.h"
#include "GroundLattice.h"
#include "TileGeodesy.h"

namespace outshine {

namespace {

[[nodiscard]] size_t PageNode(int i, int j) {
  constexpr int pageSide = Render::GroundLattice::kPageSide;
  return static_cast<size_t>(j + 1) * static_cast<size_t>(pageSide) + static_cast<size_t>(i + 1);
}

}

namespace {

struct SeamEdge {
  long StepX = 0;
  long StepY = 0;
  int FixedFine = 0;
  int FixedCoarse = 0;
  bool AlongJ = false;
};

constexpr std::array<SeamEdge, 4> kSeamEdges = {{
    {.StepX = -1,
     .StepY = 0,
     .FixedFine = 0,
     .FixedCoarse = Render::GroundLattice::kSide - 1,
     .AlongJ = true},
    {.StepX = 1,
     .StepY = 0,
     .FixedFine = Render::GroundLattice::kSide - 1,
     .FixedCoarse = 0,
     .AlongJ = true},
    {.StepX = 0,
     .StepY = -1,
     .FixedFine = 0,
     .FixedCoarse = Render::GroundLattice::kSide - 1,
     .AlongJ = false},
    {.StepX = 0,
     .StepY = 1,
     .FixedFine = Render::GroundLattice::kSide - 1,
     .FixedCoarse = 0,
     .AlongJ = false},
}};

constexpr uint64_t kTileHashMix = 0x9E3779B185EBCA87ULL;

struct SheetHash {
  [[nodiscard]] constexpr uint64_t operator()(const Data::TileId &tile) const noexcept {
    uint64_t hash = static_cast<uint32_t>(tile.Zoom);
    hash = (hash ^ tile.X) * kTileHashMix;
    return (hash ^ tile.Y) * kTileHashMix;
  }
};

using SheetIndex = FlatMap<size_t, Data::TileId, SheetHash>;

struct ZoomRange {
  int Coarsest = std::numeric_limits<int>::max();
  int Finest = std::numeric_limits<int>::min();
};

[[nodiscard]] auto SourcePriority(const Sheet &sheet) {
  const int sourceZoom = sheet.SourceZoom >= 0 ? sheet.SourceZoom : sheet.Tile.Zoom;
  return std::tuple(
      sourceZoom, -static_cast<int64_t>(sheet.Tile.Y), -static_cast<int64_t>(sheet.Tile.X));
}

[[nodiscard]] const Sheet *PeerNeighbor(const Sheet &sheet,
                                        const SeamEdge &edge,
                                        const Patchwork &laid,
                                        const SheetIndex &index) {
  long x = static_cast<long>(sheet.Tile.X) + edge.StepX;
  const long y = static_cast<long>(sheet.Tile.Y) + edge.StepY;
  if (!Ground::WrapTile(sheet.Tile.Zoom, &x, &y)) { return nullptr; }
  const size_t *found = index.Find(
      {.Zoom = sheet.Tile.Zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
  return found == nullptr ? nullptr : &laid.Sheets[*found];
}

void StitchPeers(Sheet &sheet, const Patchwork &laid, const SheetIndex &index) {
  for (const SeamEdge &edge : kSeamEdges) {
    const Sheet *peer = PeerNeighbor(sheet, edge, laid, index);
    if (peer == nullptr || SourcePriority(*peer) <= SourcePriority(sheet)) { continue; }
    for (int k = 1; k < Render::GroundLattice::kSide - 1; ++k) {
      const size_t into = edge.AlongJ ? PageNode(edge.FixedFine, k) : PageNode(k, edge.FixedFine);
      const size_t from =
          edge.AlongJ ? PageNode(edge.FixedCoarse, k) : PageNode(k, edge.FixedCoarse);
      sheet.Nodes[into] = peer->Nodes[from];
    }
  }
}

struct CornerSource {
  const Sheet *Page = nullptr;
  double I = 0;
  double J = 0;
};

[[nodiscard]] CornerSource CornerQuadrant(const Sheet &sheet,
                                          std::array<int, 2> corner,
                                          int dx,
                                          int dy,
                                          const Patchwork &laid,
                                          const SheetIndex &index,
                                          int coarsest) {
  constexpr int last = Render::GroundLattice::kSide - 1;
  const uint32_t cornerX = sheet.Tile.X + static_cast<uint32_t>(corner[0] / last);
  const uint32_t cornerY = sheet.Tile.Y + static_cast<uint32_t>(corner[1] / last);
  long x = static_cast<long>(cornerX) + dx;
  const long y = static_cast<long>(cornerY) + dy;
  if (!Ground::WrapTile(sheet.Tile.Zoom, &x, &y)) { return {}; }
  for (int zoom = sheet.Tile.Zoom; zoom >= coarsest; --zoom) {
    const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - zoom);
    const Data::TileId tile{
        .Zoom = zoom, .X = static_cast<uint32_t>(x) >> drop, .Y = static_cast<uint32_t>(y) >> drop};
    const size_t *found = index.Find(tile);
    if (found == nullptr) { continue; }
    if (drop != 0 && tile.X == sheet.Tile.X >> drop && tile.Y == sheet.Tile.Y >> drop) {
      return {};
    }
    const auto scale = static_cast<double>(uint32_t{1} << drop);
    const auto width = static_cast<double>(uint32_t{1} << static_cast<uint32_t>(zoom));
    const double u = std::fmod(static_cast<double>(cornerX) / scale - tile.X + width, width);
    const double v = static_cast<double>(cornerY) / scale - tile.Y;
    if (u != 0 && u != 1 && v != 0 && v != 1) { return {}; }
    return {.Page = &laid.Sheets[*found], .I = u * last, .J = v * last};
  }
  return {};
}

[[nodiscard]] float CornerHeight(CornerSource source) {
  constexpr int last = Render::GroundLattice::kSide - 1;
  const int i = std::min(static_cast<int>(source.I), last - 1);
  const int j = std::min(static_cast<int>(source.J), last - 1);
  const auto at = [&](int x, int y) {
    return static_cast<double>(source.Page->Nodes[PageNode(x, y)]);
  };
  const double top = std::lerp(at(i, j), at(i + 1, j), source.I - i);
  const double bottom = std::lerp(at(i, j + 1), at(i + 1, j + 1), source.I - i);
  return static_cast<float>(std::lerp(top, bottom, source.J - j));
}

[[nodiscard]] bool CoveredCorner(CornerSource source,
                                 const std::array<CornerSource, 4> &neighbors) {
  return std::ranges::any_of(neighbors, [&](CornerSource neighbor) {
    if (neighbor.Page == nullptr || neighbor.Page->Tile.Zoom <= source.Page->Tile.Zoom) {
      return false;
    }
    const auto drop = static_cast<uint32_t>(neighbor.Page->Tile.Zoom - source.Page->Tile.Zoom);
    return neighbor.Page->Tile.X >> drop == source.Page->Tile.X &&
           neighbor.Page->Tile.Y >> drop == source.Page->Tile.Y;
  });
}

[[nodiscard]] CornerSource BestCorner(const Sheet &sheet,
                                      int i,
                                      int j,
                                      const Patchwork &laid,
                                      const SheetIndex &index,
                                      int coarsest) {
  std::array<CornerSource, 4> neighbors;
  size_t at = 0;
  for (const int dx : {-1, 0}) {
    for (const int dy : {-1, 0}) {
      neighbors[at++] = CornerQuadrant(sheet, {i, j}, dx, dy, laid, index, coarsest);
    }
  }
  CornerSource best;
  for (const CornerSource next : neighbors) {
    if (next.Page == nullptr || CoveredCorner(next, neighbors)) { continue; }
    if (best.Page == nullptr || next.Page->Tile.Zoom < best.Page->Tile.Zoom ||
        (next.Page->Tile.Zoom == best.Page->Tile.Zoom &&
         SourcePriority(*next.Page) > SourcePriority(*best.Page))) {
      best = next;
    }
  }
  return best;
}

void StitchCorners(Sheet &sheet, const Patchwork &laid, const SheetIndex &index, int coarsest) {
  constexpr int last = Render::GroundLattice::kSide - 1;
  for (const int i : {0, last}) {
    for (const int j : {0, last}) {
      const CornerSource best = BestCorner(sheet, i, j, laid, index, coarsest);
      if (best.Page != nullptr) { sheet.Nodes[PageNode(i, j)] = CornerHeight(best); }
    }
  }
}

void StitchAlong(Sheet &fine,
                 const Sheet &coarse,
                 const SeamEdge &edge,
                 HeightSheets::SeamKind *kind) {
  constexpr int side = Render::GroundLattice::kSide;
  const int drop = fine.Tile.Zoom - coarse.Tile.Zoom;
  const uint32_t scale = 1u << static_cast<uint32_t>(drop);
  const uint32_t along = edge.AlongJ ? fine.Tile.Y : fine.Tile.X;
  const double offset = static_cast<double>(along % scale) * static_cast<double>(side - 1);
  const auto coarseAt = [&](int k) {
    return static_cast<double>(
        coarse.Nodes[edge.AlongJ ? PageNode(edge.FixedCoarse, k) : PageNode(k, edge.FixedCoarse)]);
  };
  for (int k = 0; k < side; ++k) {
    const double c = (offset + static_cast<double>(k)) / static_cast<double>(scale);
    const int c0 = std::min(static_cast<int>(c), side - 2);
    const double chord = std::lerp(coarseAt(c0), coarseAt(c0 + 1), c - c0);
    float &height =
        fine.Nodes[edge.AlongJ ? PageNode(edge.FixedFine, k) : PageNode(k, edge.FixedFine)];
    if (kind != nullptr && k % static_cast<int>(scale) != 0) {
      const double differenceM = std::fabs(height - chord);
      if (differenceM > kind->OddBeforeM) {
        kind->OddBeforeM = differenceM;
        const double fraction = static_cast<double>(k) / static_cast<double>(side - 1);
        const double fixed = static_cast<double>(edge.FixedFine) / static_cast<double>(side - 1);
        const Ground::Geo at = Ground::TileFracToGeo(
            {.X = static_cast<double>(fine.Tile.X) + (edge.AlongJ ? fixed : fraction),
             .Y = static_cast<double>(fine.Tile.Y) + (edge.AlongJ ? fraction : fixed)},
            fine.Tile.Zoom);
        kind->WorstLongitudeDeg = at.LongitudeDeg;
        kind->WorstLatitudeDeg = at.LatitudeDeg;
        kind->WorstFineZoom = fine.Tile.Zoom;
        kind->WorstCoarseZoom = coarse.Tile.Zoom;
      }
    }
    height = static_cast<float>(chord);
    if (kind != nullptr) {
      double &after = k % static_cast<int>(scale) == 0 ? kind->EvenM : kind->OddAfterM;
      after = std::max(after, std::fabs(height - chord));
    }
  }
}

}

namespace {

[[nodiscard]] const Sheet *CoarseNeighbor(const Sheet &fine,
                                          const SeamEdge &edge,
                                          const Patchwork &laid,
                                          const SheetIndex &index,
                                          int coarsest) {
  long nx = static_cast<long>(fine.Tile.X) + edge.StepX;
  const long ny = static_cast<long>(fine.Tile.Y) + edge.StepY;
  if (!Ground::WrapTile(fine.Tile.Zoom, &nx, &ny)) { return nullptr; }
  if (index.Find({.Zoom = fine.Tile.Zoom,
                  .X = static_cast<uint32_t>(nx),
                  .Y = static_cast<uint32_t>(ny)}) != nullptr) {
    return nullptr;
  }
  const uint32_t boundary = edge.AlongJ ? fine.Tile.X + (edge.StepX > 0 ? 1u : 0u)
                                        : fine.Tile.Y + (edge.StepY > 0 ? 1u : 0u);
  for (int zoom = fine.Tile.Zoom - 1; zoom >= coarsest; --zoom) {
    const auto drop = static_cast<uint32_t>(fine.Tile.Zoom - zoom);
    if (boundary % (1u << drop) != 0) { break; }
    const Data::TileId wanted{.Zoom = zoom,
                              .X = static_cast<uint32_t>(nx) >> drop,
                              .Y = static_cast<uint32_t>(ny) >> drop};
    if (const size_t *found = index.Find(wanted)) { return &laid.Sheets[*found]; }
  }
  return nullptr;
}

[[nodiscard]] bool
IndexSheets(const Patchwork &laid, SheetIndex &index, ZoomRange &zooms, std::string &error) {
  for (size_t i = 0; i < laid.Sheets.size(); ++i) {
    if (laid.Sheets[i].Nodes.size() != Render::GroundLattice::kPageNodes) { continue; }
    const auto added = index.Emplace(laid.Sheets[i].Tile, i);
    if (!added) {
      error = added.error() == FlatMapError::AllocationFailed
                  ? "ground stitch index allocation failed"
                  : "ground stitch index capacity exceeded";
      return false;
    }
    if (!added->second) {
      error = "ground patchwork repeats tile";
      return false;
    }
    zooms.Coarsest = std::min(zooms.Coarsest, laid.Sheets[i].Tile.Zoom);
    zooms.Finest = std::max(zooms.Finest, laid.Sheets[i].Tile.Zoom);
  }
  return true;
}

void StitchAtZoom(Patchwork &laid,
                  const SheetIndex &index,
                  ZoomRange zooms,
                  int zoom,
                  HeightSheets::Seam &seams) {
  for (size_t sheetIndex = 0; sheetIndex < laid.Sheets.size(); ++sheetIndex) {
    Sheet &fine = laid.Sheets[sheetIndex];
    if (fine.Tile.Zoom != zoom || fine.Nodes.size() != Render::GroundLattice::kPageNodes) {
      continue;
    }
    for (const SeamEdge &edge : kSeamEdges) {
      const Sheet *coarse = CoarseNeighbor(fine, edge, laid, index, zooms.Coarsest);
      if (coarse == nullptr) { continue; }
      HeightSheets::SeamKind *kind = fine.Virtual && coarse->Virtual ? &seams.Virtual : &seams.Real;
      ++kind->Edges;
      StitchAlong(fine, *coarse, edge, kind);
    }
  }
}

}

bool HeightSheets::StitchEdges(Patchwork &laid, std::string &error) {
  Seams_ = {};
  SheetIndex index;
  ZoomRange zooms;
  if (!IndexSheets(laid, index, zooms, error)) { return false; }
  for (int zoom = zooms.Coarsest; zoom <= zooms.Finest; ++zoom) {
    for (Sheet &sheet : laid.Sheets) {
      if (sheet.Tile.Zoom == zoom && sheet.Nodes.size() == Render::GroundLattice::kPageNodes) {
        StitchPeers(sheet, laid, index);
      }
    }
    StitchAtZoom(laid, index, zooms, zoom, Seams_);
    for (Sheet &sheet : laid.Sheets) {
      if (sheet.Tile.Zoom == zoom && sheet.Nodes.size() == Render::GroundLattice::kPageNodes) {
        StitchCorners(sheet, laid, index, zooms.Coarsest);
      }
    }
  }
  return true;
}

}
