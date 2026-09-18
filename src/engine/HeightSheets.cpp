#include "HeightSheets.h"
#include <expected>

#include "Digest.h"
#include "math/RenderFrame.h"

#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <vector>

#include "ChunkSurface.h"
#include "Geodesy.h"
#include "TerrainGrid.h"
#include "GroundLattice.h"
#include "SceneRenderer.h"
#include "TileGeodesy.h"
#include "math/Vec3.h"

namespace outshine {

static_assert(kPatchGrid + 1 == Render::GroundLattice::kSide,
              "a page holds the nodes the patchwork's chunk was built from, so the lattice draws "
              "the surface the roads are draped on");

namespace {

[[nodiscard]] Vec3 EcefOf(double lonDeg, double latDeg) {
  const Ground::Ecef at =
      Ground::GeoToEcefWgs84({.LongitudeDeg = lonDeg, .LatitudeDeg = latDeg, .HeightM = 0.0});
  return {{at.X, at.Y, at.Z}};
}

[[nodiscard]] double Dot(const Vec3 &a, const Vec3 &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

[[nodiscard]] float Dot2(std::array<float, 2> v) {
  return v[0] * v[0] + v[1] * v[1];
}

}

std::expected<Render::HeightPageHandle, std::string>
HeightSheets::PageFor(Data::TileId tile, std::span<const float> nodes) {
  const auto key = std::tuple{tile.Zoom, tile.X, tile.Y};
  const auto found = PageIndex_.find(key);
  if (found != PageIndex_.end()) {
    Held &one = Held_[found->second];
    if (one.Page && std::ranges::equal(one.Nodes, nodes)) { return one.Page; }
    std::vector<float> replacement(nodes.begin(), nodes.end());
    const auto page = Renderer_->PlaceHeightPage(replacement);
    if (!page) { return std::unexpected(page.error()); }
    Renderer_->ReleaseHeightPage(one.Page);
    one.Page = *page;
    one.Nodes = std::move(replacement);
    return one.Page;
  }
  std::vector<float> owned(nodes.begin(), nodes.end());
  const auto page = Renderer_->PlaceHeightPage(owned);
  if (!page) { return std::unexpected(page.error()); }
  PageIndex_.emplace(key, Held_.size());
  Held_.push_back({.Tile = tile, .Page = *page, .Nodes = std::move(owned)});
  return *page;
}

Render::TerrainTile HeightSheets::TileOf(Data::TileId tile,
                                         Render::HeightPageHandle page,
                                         std::span<const float> nodes) const {
  const Ground::GeoBounds bounds = Ground::TileBounds(tile);
  const double midLon = 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg);
  const double midLat = 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg);
  const Vec3 centre = EcefOf(midLon, midLat);
  const EnuAxes tile_ =
      EnuAxesEcef({.LongitudeDeg = midLon, .LatitudeDeg = midLat, .HeightM = 0.0});
  const auto corner = [&](double lonDeg, double latDeg) {
    const Vec3 away = EcefOf(lonDeg, latDeg) - centre;
    return std::array<float, 2>{
        {static_cast<float>(Dot(away, tile_.East)), static_cast<float>(Dot(away, tile_.North))}};
  };
  const std::array<float, 2> nw = corner(bounds.MinLonDeg, bounds.MaxLatDeg);
  const std::array<float, 2> ne = corner(bounds.MaxLonDeg, bounds.MaxLatDeg);
  const std::array<float, 2> sw = corner(bounds.MinLonDeg, bounds.MinLatDeg);
  const std::array<float, 2> se = corner(bounds.MaxLonDeg, bounds.MinLatDeg);

  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  const Vec3 &up = Frame_.UpEcef();
  const EastNorthUp at = Frame_.Place(centre);
  Render::TerrainTile made;
  const std::array<const Vec3 *, 3> axes = {{&tile_.East, &tile_.North, &tile_.Up}};
  for (size_t column = 0; column < 3; ++column) {
    made.Row[column * 4u] = static_cast<float>(Dot(east, *axes[column]));
    made.Row[column * 4u + 1u] = static_cast<float>(Dot(up, *axes[column]));
    made.Row[column * 4u + 2u] =
        static_cast<float>(RenderFrame::ZOfNorth(Dot(north, *axes[column])));
    made.Row[column * 4u + 3u] = 0.0f;
  }
  made.Row[12] = static_cast<float>(at.EastM);
  made.Row[13] = static_cast<float>(at.UpM);
  made.Row[14] = static_cast<float>(RenderFrame::ZOfNorth(at.NorthM));
  made.Row[15] = 1.0f;
  made.Corners = {{nw[0], nw[1], ne[0], ne[1], sw[0], sw[1], se[0], se[1]}};
  made.Page = page;
  made.SagInv = static_cast<float>(1.0 / std::sqrt(Dot(centre, centre)));
  const auto steps = static_cast<float>(Render::GroundLattice::kSide - 1);
  made.StepE = 0.5f * ((ne[0] - nw[0]) + (se[0] - sw[0])) / steps;
  made.StepN = 0.5f * ((nw[1] - sw[1]) + (ne[1] - se[1])) / steps;
  const auto [low, high] = std::ranges::minmax_element(nodes);
  const float skirt = Render::GroundLattice::kSkirtSteps * std::max(made.StepE, made.StepN);
  const float sag = 0.5f * std::max({Dot2(nw), Dot2(ne), Dot2(sw), Dot2(se)}) * made.SagInv;
  made.LowM = (nodes.empty() ? 0.0f : *low) - skirt - sag;
  made.HighM = nodes.empty() ? 0.0f : *high;
  return made;
}

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

namespace {

constexpr long kBlockTiles = 8;
constexpr long kHalfBlock = kBlockTiles / 2;

struct Block {
  long X0 = 0;
  long Y0 = 0;
};

[[nodiscard]] Block BlockAround(LongitudeLatitude eye, int zoom) {
  const Ground::TileFrac at = Ground::ToTileFracClamped(
      Ground::Geo{.LongitudeDeg = eye.LongitudeDeg, .LatitudeDeg = eye.LatitudeDeg}, zoom);
  const auto origin = [](double f) {
    return kHalfBlock *
           static_cast<long>(std::floor((std::floor(f) - static_cast<double>(kHalfBlock - 1)) /
                                        static_cast<double>(kHalfBlock)));
  };
  return {.X0 = origin(at.X), .Y0 = origin(at.Y)};
}

[[nodiscard]] bool Covers(Block finer, long x, long y) {
  return x >= finer.X0 / 2 && x < finer.X0 / 2 + kHalfBlock && y >= finer.Y0 / 2 &&
         y < finer.Y0 / 2 + kHalfBlock;
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

size_t HeightSheets::Refine(Patchwork &laid, Nearer how) {
  if (how.Levels <= 0) { return 0; }
  std::vector<Sheet> made;
  Block finer = BlockAround(how.Eye, how.FinestZoom + how.Levels + 1);
  for (int level = how.Levels; level >= 1; --level) {
    const int zoom = how.FinestZoom + level;
    const Block block = BlockAround(how.Eye, zoom);
    const bool anyFiner = level < how.Levels;
    for (long row = 0; row < kBlockTiles; ++row) {
      for (long column = 0; column < kBlockTiles; ++column) {
        const long x = block.X0 + column;
        const long y = block.Y0 + row;
        if (anyFiner && Covers(finer, x, y)) { continue; }
        const Data::TileId tile{
            .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
        made.push_back({.Tile = tile,
                        .Nodes = {},
                        .Side = Render::GroundLattice::kSide,
                        .Postings = 0,
                        .Virtual = true});
      }
    }
    finer = block;
  }
  std::erase_if(laid.Sheets, [&](const Sheet &one) {
    return one.Tile.Zoom == how.FinestZoom &&
           Covers(finer, static_cast<long>(one.Tile.X), static_cast<long>(one.Tile.Y));
  });
  const size_t count = made.size();
  for (Sheet &one : made) { laid.Sheets.push_back(std::move(one)); }
  return count;
}

bool HeightSheets::HandsGrid(const Patchwork &laid, std::string &error) {
  for (const Sheet &sheet : laid.Sheets) {
    if (sheet.Side != Render::GroundLattice::kSide || sheet.Virtual || sheet.Postings < 2 ||
        sheet.Postings == GridPostings_) {
      continue;
    }
    std::vector<float> fractions;
    fractions.reserve(static_cast<size_t>(Render::GroundLattice::kSide));
    for (int k = 0; k < Render::GroundLattice::kSide; ++k) {
      fractions.push_back(
          static_cast<float>(FractionOf(k, sheet.Postings, Render::GroundLattice::kSide)));
    }
    if (!Renderer_->SetGroundGrid(fractions, error)) { return false; }
    GridPostings_ = sheet.Postings;
    return true;
  }
  return true;
}

bool HeightSheets::Hands(Patchwork &laid, std::string &error) {
  if (Renderer_ == nullptr || !Framed_) { return true; }
  std::map<SheetKey, size_t> wanted;
  for (size_t i = 0; i < laid.Sheets.size(); ++i) { wanted.emplace(KeyOf(laid.Sheets[i].Tile), i); }
  std::erase_if(Held_, [&](const Held &one) {
    if (wanted.contains(KeyOf(one.Tile))) { return false; }
    Renderer_->ReleaseHeightPage(one.Page);
    return true;
  });
  PageIndex_.clear();
  for (size_t i = 0; i < Held_.size(); ++i) { PageIndex_.emplace(KeyOf(Held_[i].Tile), i); }
  Flat_ = 0;
  StitchEdges(laid);
  Instances_.clear();
  Virtual_.clear();
  const size_t nodes = Render::GroundLattice::kPageNodes;
  for (const Sheet &sheet : laid.Sheets) {
    if (sheet.Side != Render::GroundLattice::kSide || sheet.Nodes.size() != nodes) {
      ++Flat_;
      continue;
    }
    const auto page = PageFor(sheet.Tile, sheet.Nodes);
    if (!page) {
      error = page.error();
      return false;
    }
    (sheet.Virtual ? Virtual_ : Instances_).push_back(TileOf(sheet.Tile, *page, sheet.Nodes));
  }
  if (!HandsGrid(laid, error)) { return false; }
  return Renderer_->SetTerrainTiles(Instances_, Virtual_, error);
}

std::optional<double>
HeightSheets::FieldUpM(const Ground::GroundStream &ground, int zoom, EastNorth at) {
  if (!Framed_) { return std::nullopt; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  Vec3 ecef;
  for (int axis = 0; axis < 3; ++axis) {
    ecef[axis] = origin[axis] + at.EastM * east[axis] + at.NorthM * north[axis];
  }
  const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
  const Ground::TileFrac frac = Ground::ToTileFracClamped(
      {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}, zoom);
  const Data::TileId tile{.Zoom = zoom,
                          .X = static_cast<uint32_t>(std::floor(frac.X)),
                          .Y = static_cast<uint32_t>(std::floor(frac.Y))};
  const Ground::TerrainField *field = FieldAt(ground, tile);
  if (field == nullptr || !field->Meshable()) { return std::nullopt; }
  const double aslM =
      field->PostingM({.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
  return Frame_
      .Place({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = aslM})
      .UpM;
}

void HeightSheets::Clear() {
  if (Renderer_ != nullptr) {
    for (const Held &one : Held_) { Renderer_->ReleaseHeightPage(one.Page); }
    std::string ignored;
    (void)Renderer_->SetTerrainTiles({}, {}, ignored);
  }
  Renderer_ = nullptr;
  Held_.clear();
  PageIndex_.clear();
  Instances_.clear();
  Virtual_.clear();
  Fields_.clear();
  GridPostings_ = 0;
}

uint64_t HeightSheets::Digest() const {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint32_t word) { digest = (digest ^ word) * kDigestPrime; };
  const auto foldFloat = [&fold](float value) { fold(std::bit_cast<uint32_t>(value)); };
  const auto foldPage = [&fold](Render::HeightPageHandle page) {
    fold(page.Slot);
    fold(static_cast<uint32_t>(page.Generation));
    fold(static_cast<uint32_t>(page.Generation >> 32u));
  };
  for (const std::vector<Render::TerrainTile> *tiles : {&Instances_, &Virtual_}) {
    for (const Render::TerrainTile &one : *tiles) {
      for (const float value : one.Row) { foldFloat(value); }
      for (const float value : one.Corners) { foldFloat(value); }
      foldPage(one.Page);
      foldFloat(one.SagInv);
      foldFloat(one.StepE);
      foldFloat(one.StepN);
      foldFloat(one.LowM);
      foldFloat(one.HighM);
    }
  }
  for (const Held &held : Held_) {
    fold(static_cast<uint32_t>(held.Tile.Zoom));
    fold(static_cast<uint32_t>(held.Tile.X));
    fold(static_cast<uint32_t>(held.Tile.Y));
    foldPage(held.Page);
    for (const float value : held.Nodes) { foldFloat(value); }
  }
  return digest;
}

}
