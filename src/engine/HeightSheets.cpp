#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <vector>

#include "ChunkSurface.h"
#include "Geodesy.h"
#include "TerrainGrid.h"
#include "GroundLattice.h"
#include "TileGeodesy.h"
#include "math/Vec3.h"

namespace outshine {

static_assert(kPatchGrid + 1 == Render::GroundLattice::kSide,
              "a page holds the nodes the patchwork's chunk was built from, so the lattice draws "
              "the surface the roads are draped on");

namespace {

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
}

[[nodiscard]] int SourceZoomOf(const Sheet &sheet, int finestZoom) {
  if (sheet.SourceZoom >= 0) { return sheet.SourceZoom; }
  return sheet.Virtual ? finestZoom : sheet.Tile.Zoom;
}

[[nodiscard]] double NodeFraction(const Sheet &sheet, int k) {
  constexpr int side = Render::GroundLattice::kSide;
  if (sheet.Virtual || sheet.SourceZoom >= 0) {
    return static_cast<double>(k) / static_cast<double>(side - 1);
  }
  if (k < 0) { return -FractionOf(1, sheet.Postings, side); }
  if (k >= side) { return 2.0 - FractionOf(side - 2, sheet.Postings, side); }
  return FractionOf(k, sheet.Postings, side);
}

[[nodiscard]] size_t PageNode(int i, int j) {
  constexpr int pageSide = Render::GroundLattice::kPageSide;
  return static_cast<size_t>(j + 1) * static_cast<size_t>(pageSide) + static_cast<size_t>(i + 1);
}

void CopiesEdgeIntoRim(std::vector<float> &page, const std::vector<bool> &missing) {
  constexpr int side = Render::GroundLattice::kSide;
  for (int j = -1; j <= side; ++j) {
    for (int i = -1; i <= side; ++i) {
      if (!missing[PageNode(i, j)]) { continue; }
      page[PageNode(i, j)] = page[PageNode(std::clamp(i, 0, side - 1), std::clamp(j, 0, side - 1))];
    }
  }
}

}

const Ground::TerrainField *HeightSheets::FieldAt(const Ground::GroundStream &ground,
                                                  Data::TileId tile) {
  for (const auto &one : Fields_) {
    if (one.first == tile) { return one.second.get(); }
  }
  Fields_.emplace_back(tile, ground.StitchedFieldAwaited(tile));
  return Fields_.back().second.get();
}

const Ground::TerrainField *HeightSheets::HeldFieldAt(Data::TileId tile) const {
  for (const auto &one : Fields_) {
    if (one.first == tile) { return one.second.get(); }
  }
  return nullptr;
}

std::optional<float>
HeightSheets::AslAt(const Ground::GroundStream &ground, int zoom, Ground::TileFrac at) {
  long x = static_cast<long>(std::floor(at.X));
  const long y = static_cast<long>(std::floor(at.Y));
  const double col = at.X - static_cast<double>(x);
  const double row = at.Y - static_cast<double>(y);
  if (!Ground::WrapTile(zoom, &x, &y)) { return std::nullopt; }
  const Ground::TerrainField *field =
      FieldAt(ground, {.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
  if (field == nullptr || !field->Meshable()) { return std::nullopt; }
  return field->PostingM({.Col = col, .Row = row});
}

bool HeightSheets::HaloOf(Sheet &sheet, const Ground::GroundStream &ground, int finestZoom) {
  constexpr int side = Render::GroundLattice::kSide;
  const bool whole = (sheet.Virtual || sheet.SourceZoom >= 0) &&
                     sheet.Nodes.size() != Render::GroundLattice::kNodes;
  if (!whole && sheet.Nodes.size() != Render::GroundLattice::kNodes) { return false; }
  std::vector<float> page(Render::GroundLattice::kPageNodes, 0.0f);
  const int sourceZoom = SourceZoomOf(sheet, finestZoom);
  const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - sourceZoom);
  const int zoom = sheet.Tile.Zoom - static_cast<int>(drop);
  const double span = 1.0 / static_cast<double>(1u << drop);
  const double atX = static_cast<double>(sheet.Tile.X >> drop) +
                     static_cast<double>(sheet.Tile.X & ((1u << drop) - 1u)) * span;
  const double atY = static_cast<double>(sheet.Tile.Y >> drop) +
                     static_cast<double>(sheet.Tile.Y & ((1u << drop) - 1u)) * span;
  std::vector<bool> missing(Render::GroundLattice::kPageNodes, false);
  bool anyMissing = false;
  for (int j = -1; j <= side; ++j) {
    for (int i = -1; i <= side; ++i) {
      const bool inside = i >= 0 && i < side && j >= 0 && j < side;
      if (inside && !whole) {
        page[PageNode(i, j)] =
            sheet
                .Nodes[static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i)];
        continue;
      }
      const std::optional<float> asl = AslAt(
          ground,
          zoom,
          {.X = atX + span * NodeFraction(sheet, i), .Y = atY + span * NodeFraction(sheet, j)});
      if (asl) {
        page[PageNode(i, j)] = *asl;
      } else if (inside) {
        return false;
      } else {
        missing[PageNode(i, j)] = true;
        anyMissing = true;
      }
    }
  }
  if (anyMissing) {
    CopiesEdgeIntoRim(page, missing);
    ++RimsMissing_;
  }
  sheet.Nodes = std::move(page);
  return true;
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

using SheetKey = std::tuple<int, uint32_t, uint32_t>;

[[nodiscard]] SheetKey KeyOf(Data::TileId tile) {
  return {tile.Zoom, tile.X, tile.Y};
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
      kind->OddBeforeM = std::max(kind->OddBeforeM, std::fabs(height - chord));
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
                                          const std::map<SheetKey, size_t> &index) {
  long nx = static_cast<long>(fine.Tile.X) + edge.StepX;
  const long ny = static_cast<long>(fine.Tile.Y) + edge.StepY;
  if (!Ground::WrapTile(fine.Tile.Zoom, &nx, &ny)) { return nullptr; }
  const int coarsest = std::get<0>(index.begin()->first);
  for (int zoom = fine.Tile.Zoom; zoom >= coarsest; --zoom) {
    const auto drop = static_cast<uint32_t>(fine.Tile.Zoom - zoom);
    const SheetKey wanted{
        zoom, static_cast<uint32_t>(nx) >> drop, static_cast<uint32_t>(ny) >> drop};
    const auto found = index.find(wanted);
    if (found != index.end()) {
      return wanted < KeyOf(fine.Tile) ? &laid.Sheets[found->second] : nullptr;
    }
  }
  return nullptr;
}

}

void HeightSheets::StitchEdges(Patchwork &laid) {
  Seams_ = {};
  std::map<SheetKey, size_t> index;
  for (size_t i = 0; i < laid.Sheets.size(); ++i) {
    if (laid.Sheets[i].Nodes.size() == Render::GroundLattice::kPageNodes) {
      index.emplace(KeyOf(laid.Sheets[i].Tile), i);
    }
  }
  for (const auto &[key, sheetIndex] : index) {
    Sheet &fine = laid.Sheets[sheetIndex];
    for (const SeamEdge &edge : kSeamEdges) {
      const Sheet *coarse = CoarseNeighbor(fine, edge, laid, index);
      if (coarse == nullptr) { continue; }
      SeamKind *kind = nullptr;
      if (coarse->Tile.Zoom < fine.Tile.Zoom) {
        kind = fine.Virtual && coarse->Virtual ? &Seams_.Virtual : &Seams_.Real;
        ++kind->Edges;
      }
      StitchAlong(fine, *coarse, edge, kind);
    }
  }
}

void HeightSheets::AsksFields(const Ground::GroundStream &ground,
                              const Patchwork &laid,
                              int finestZoom) {
  for (const Sheet &sheet : laid.Sheets) {
    const int sourceZoom = SourceZoomOf(sheet, finestZoom);
    const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - sourceZoom);
    const int zoom = sheet.Tile.Zoom - static_cast<int>(drop);
    const long x = static_cast<long>(sheet.Tile.X >> drop);
    const long y = static_cast<long>(sheet.Tile.Y >> drop);
    for (long dy = -1; dy <= 1; ++dy) {
      for (long dx = -1; dx <= 1; ++dx) {
        long nx = x + dx;
        const long ny = y + dy;
        if (ny < 0 || !Ground::WrapTile(zoom, &nx, &ny)) { continue; }
        (void)ground.StitchedField(
            {.Zoom = zoom, .X = static_cast<uint32_t>(nx), .Y = static_cast<uint32_t>(ny)});
      }
    }
  }
}

size_t HeightSheets::Halos(Patchwork &laid, const Ground::GroundStream &ground, int finestZoom) {
  RimsMissing_ = 0;
  size_t haloed = 0;
  AsksFields(ground, laid, finestZoom);
  for (Sheet &sheet : laid.Sheets) {
    if (sheet.Side == Render::GroundLattice::kSide && HaloOf(sheet, ground, finestZoom)) {
      ++haloed;
    }
  }
  return haloed;
}

bool HeightSheets::Hands(Patchwork &laid, std::string &error) {
  if (!Framed_) { return true; }
  StitchEdges(laid);
  return Residency_.Publish(laid, Frame_, error);
}

std::optional<double> HeightSheets::FieldUpM(int zoom, EastNorth at) const {
  if (!Framed_) { return std::nullopt; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  Vec3 ecef;
  for (int axis = 0; axis < 3; ++axis) {
    ecef[axis] = origin[axis] + at.EastM * east[axis] + at.NorthM * north[axis];
  }
  const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
  for (int heldZoom = zoom; heldZoom >= 0; --heldZoom) {
    const Ground::TileFrac frac = Ground::ToTileFracClamped(
        {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}, heldZoom);
    const Data::TileId tile{.Zoom = heldZoom,
                            .X = static_cast<uint32_t>(std::floor(frac.X)),
                            .Y = static_cast<uint32_t>(std::floor(frac.Y))};
    const Ground::TerrainField *field = HeldFieldAt(tile);
    if (field == nullptr || !field->Meshable()) { continue; }
    const double aslM =
        field->PostingM({.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
    return Frame_
        .Place({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = aslM})
        .UpM;
  }
  return std::nullopt;
}

void HeightSheets::Clear() {
  Residency_.Clear();
  Fields_.clear();
}

uint64_t HeightSheets::Digest() const {
  return Residency_.Digest();
}

size_t HeightSheets::HeapBytes() const noexcept {
  return Residency_.HeapBytes() + Fields_.capacity() * sizeof(decltype(Fields_)::value_type);
}

}
