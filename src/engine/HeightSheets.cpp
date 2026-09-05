#include "HeightSheets.h"
#include "math/RenderFrame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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
#include "Live.h"
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

} // namespace

Render::PageId
HeightSheets::PageFor(Data::TileId tile, std::span<const float> nodes, std::string &error) {
  for (Held &one : Held_) {
    if (one.Tile == tile) {
      if (one.Nodes != std::vector<float>(nodes.begin(), nodes.end())) {
        Live_->ReleaseHeightPage(one.Page);
        one.Page = Live_->PlaceHeightPage(nodes, error);
        one.Nodes.assign(nodes.begin(), nodes.end());
      }
      one.Wanted = one.Page != Render::kNoPage;
      return one.Page;
    }
  }
  const Render::PageId page = Live_->PlaceHeightPage(nodes, error);
  if (page == Render::kNoPage) { return page; }
  Held_.push_back({.Tile = tile,
                   .Page = page,
                   .Wanted = true,
                   .Nodes = std::vector<float>(nodes.begin(), nodes.end())});
  return page;
}

Render::GroundTile HeightSheets::TileOf(Data::TileId tile,
                                        Render::PageId page,
                                        std::span<const float> nodes,
                                        std::array<float, 4> stitched) const {
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
  Render::GroundInstance made;
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
  made.Page = static_cast<float>(page);
  made.SagInv = static_cast<float>(1.0 / std::sqrt(Dot(centre, centre)));
  const auto steps = static_cast<float>(Render::GroundLattice::kSide - 1);
  made.StepE = 0.5f * ((ne[0] - nw[0]) + (se[0] - sw[0])) / steps;
  made.StepN = 0.5f * ((nw[1] - sw[1]) + (ne[1] - se[1])) / steps;
  made.Stitched = stitched;
  const auto [low, high] = std::ranges::minmax_element(nodes);
  const float skirt = Render::GroundLattice::kSkirtSteps * std::max(made.StepE, made.StepN);
  const float sag = 0.5f * std::max({Dot2(nw), Dot2(ne), Dot2(sw), Dot2(se)}) * made.SagInv;
  return {.Instance = made,
          .LowM = (nodes.empty() ? 0.0f : *low) - skirt - sag,
          .HighM = nodes.empty() ? 0.0f : *high};
}

namespace {

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
}

[[nodiscard]] double NodeFraction(const Sheet &sheet, int k) {
  constexpr int side = Render::GroundLattice::kSide;
  if (sheet.Virtual) { return static_cast<double>(k) / static_cast<double>(side - 1); }
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

} // namespace

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

} // namespace

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
  const bool whole = sheet.Virtual && sheet.Nodes.size() != Render::GroundLattice::kNodes;
  if (!whole && sheet.Nodes.size() != Render::GroundLattice::kNodes) { return false; }
  std::vector<float> page(Render::GroundLattice::kPageNodes, 0.0f);
  const auto drop = sheet.Virtual ? static_cast<uint32_t>(sheet.Tile.Zoom - finestZoom) : 0u;
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

void MeasuresSeamAlong(const Sheet &fine,
                       const Sheet &coarse,
                       const SeamEdge &edge,
                       HeightSheets::SeamKind &kind) {
  constexpr int side = Render::GroundLattice::kSide;
  constexpr int half = (side - 1) / 2;
  const long along = edge.AlongJ ? static_cast<long>(fine.Tile.Y) : static_cast<long>(fine.Tile.X);
  const int coarseFrom = static_cast<int>(along % 2) * half;
  const auto fineAt = [&](int k) {
    const size_t node = edge.AlongJ ? PageNode(edge.FixedFine, k) : PageNode(k, edge.FixedFine);
    return static_cast<double>(fine.Nodes[node]);
  };
  const auto coarseAt = [&](int c) {
    const size_t node = edge.AlongJ ? PageNode(edge.FixedCoarse, c) : PageNode(c, edge.FixedCoarse);
    return static_cast<double>(coarse.Nodes[node]);
  };
  for (int k = 0; k < side; ++k) {
    if (k % 2 == 0) {
      kind.EvenM = std::max(kind.EvenM, std::fabs(fineAt(k) - coarseAt(coarseFrom + k / 2)));
      continue;
    }
    const double chord = 0.5 * (coarseAt(coarseFrom + k / 2) + coarseAt(coarseFrom + k / 2 + 1));
    const double snapped = 0.5 * (fineAt(k - 1) + fineAt(k + 1));
    kind.OddBeforeM = std::max(kind.OddBeforeM, std::fabs(fineAt(k) - chord));
    kind.OddAfterM = std::max(kind.OddAfterM, std::fabs(snapped - chord));
  }
}

} // namespace

void HeightSheets::MeasuresSeams(const Sheet &fine,
                                 const Patchwork &laid,
                                 std::array<float, 4> &stitched) {
  if (fine.Nodes.size() != Render::GroundLattice::kPageNodes) { return; }
  for (size_t at = 0; at < kSeamEdges.size(); ++at) {
    const SeamEdge &edge = kSeamEdges[at];
    const long nx = static_cast<long>(fine.Tile.X) + edge.StepX;
    const long ny = static_cast<long>(fine.Tile.Y) + edge.StepY;
    if (nx < 0 || ny < 0) { continue; }
    const Data::TileId coarse{.Zoom = fine.Tile.Zoom - 1,
                              .X = static_cast<uint32_t>(nx / 2),
                              .Y = static_cast<uint32_t>(ny / 2)};
    const auto found = std::ranges::find_if(
        laid.Sheets, [&coarse](const Sheet &one) { return one.Tile == coarse; });
    if (found == laid.Sheets.end() || found->Nodes.size() != Render::GroundLattice::kPageNodes) {
      continue;
    }
    stitched[at] = 1.0f;
    SeamKind &kind = fine.Virtual && found->Virtual ? Seams_.Virtual : Seams_.Real;
    ++kind.Edges;
    MeasuresSeamAlong(fine, *found, edge, kind);
  }
}

void HeightSheets::AsksFields(const Ground::GroundStream &ground,
                              const Patchwork &laid,
                              int finestZoom) {
  for (const Sheet &sheet : laid.Sheets) {
    const auto drop = sheet.Virtual ? static_cast<uint32_t>(sheet.Tile.Zoom - finestZoom) : 0u;
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
        made.push_back(
            {.Tile = tile, .Side = Render::GroundLattice::kSide, .Postings = 0, .Virtual = true});
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

HeightSheets::Soup HeightSheets::SoupOf(const Patchwork &laid, int zoomAtLeast) const {
  constexpr int side = Render::GroundLattice::kSide;
  Soup out;
  out.TallestM = -kBeyondAnyCoordinate;
  out.LowestM = kBeyondAnyCoordinate;
  if (!Framed_) { return out; }
  for (const Sheet &one : laid.Sheets) {
    if (one.Side != side || (!one.Virtual && one.Postings < 2) || one.Tile.Zoom < zoomAtLeast ||
        one.Nodes.size() != Render::GroundLattice::kPageNodes) {
      continue;
    }
    const auto first = static_cast<uint32_t>(out.PositionM.size() / 3u);
    for (int j = 0; j < side; ++j) {
      const double fy = NodeFraction(one, j);
      for (int i = 0; i < side; ++i) {
        const double fx = NodeFraction(one, i);
        const Ground::Geo geo = Ground::TileFracToGeo(
            {.X = static_cast<double>(one.Tile.X) + fx, .Y = static_cast<double>(one.Tile.Y) + fy},
            one.Tile.Zoom);
        const auto heightM = static_cast<double>(one.Nodes[PageNode(i, j)]);
        const EastNorthUp stood = Frame_.Place(
            {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = heightM});
        out.PositionM.push_back(static_cast<float>(stood.EastM));
        out.PositionM.push_back(static_cast<float>(stood.UpM));
        out.PositionM.push_back(static_cast<float>(RenderFrame::ZOfNorth(stood.NorthM)));
        if (heightM > out.TallestM) {
          out.TallestM = heightM;
          out.TallestOutM = std::hypot(stood.EastM, stood.NorthM);
        }
        out.LowestM = std::min(out.LowestM, heightM);
      }
    }
    const auto at = [first](int i, int j) {
      return first + static_cast<uint32_t>(j) * static_cast<uint32_t>(side) +
             static_cast<uint32_t>(i);
    };
    for (int j = 0; j + 1 < side; ++j) {
      for (int i = 0; i + 1 < side; ++i) {
        out.Index.insert(
            out.Index.end(),
            {at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i, j), at(i + 1, j + 1), at(i + 1, j)});
      }
    }
  }
  if (out.PositionM.empty()) {
    out.TallestM = 0.0;
    out.LowestM = 0.0;
  }
  return out;
}

HeightSheets::Pressed
HeightSheets::Press(std::span<const Yields> yields, Patchwork &laid, double mostEarthworkM) const {
  if (!Framed_ || yields.empty()) { return {}; }
  const int side = Render::GroundLattice::kSide;
  std::vector<EastNorth> at;
  std::vector<double> up;
  std::vector<std::pair<size_t, size_t>> where;
  for (size_t sheet = 0; sheet < laid.Sheets.size(); ++sheet) {
    const Sheet &one = laid.Sheets[sheet];
    if (one.Side != side || (!one.Virtual && one.Postings < 2) ||
        one.Nodes.size() != Render::GroundLattice::kPageNodes) {
      continue;
    }
    for (int j = -1; j <= side; ++j) {
      const double fy = NodeFraction(one, j);
      for (int i = -1; i <= side; ++i) {
        const double fx = NodeFraction(one, i);
        const Ground::Geo geo = Ground::TileFracToGeo(
            {.X = static_cast<double>(one.Tile.X) + fx, .Y = static_cast<double>(one.Tile.Y) + fy},
            one.Tile.Zoom);
        const size_t node = PageNode(i, j);
        const EastNorthUp stood = Frame_.Place({.LongitudeDeg = geo.LongitudeDeg,
                                                .LatitudeDeg = geo.LatitudeDeg,
                                                .HeightM = static_cast<double>(one.Nodes[node])});
        at.push_back({.EastM = stood.EastM, .NorthM = stood.NorthM});
        up.push_back(stood.UpM);
        where.emplace_back(sheet, node);
      }
    }
  }
  std::vector<double> was(up);
  const outshine::Pressed pressed = PressPoints(yields, at, up, mostEarthworkM);
  Pressed told{.Nodes = pressed.Moved, .Structures = pressed.Structures, .Held = pressed.Held};
  if (pressed.Moved == 0) { return told; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  const Vec3 &upward = Frame_.UpEcef();
  for (size_t one = 0; one < up.size(); ++one) {
    if (up[one] == was[one]) { continue; }
    told.DeepestM = std::max(told.DeepestM, was[one] - up[one]);
    told.RaisedM = std::max(told.RaisedM, up[one] - was[one]);
    const double e = at[one].EastM;
    const double n = at[one].NorthM;
    Vec3 ecef;
    for (int axis = 0; axis < 3; ++axis) {
      ecef[axis] = origin[axis] + e * east[axis] + n * north[axis] + up[one] * upward[axis];
    }
    const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
    laid.Sheets[where[one].first].Nodes[where[one].second] = static_cast<float>(geo.HeightM);
  }
  std::vector<double> written(up.size());
  for (size_t one = 0; one < up.size(); ++one) {
    const Sheet &sheet = laid.Sheets[where[one].first];
    constexpr size_t pageSide = Render::GroundLattice::kPageSide;
    const int i = static_cast<int>(where[one].second % pageSide) - 1;
    const int j = static_cast<int>(where[one].second / pageSide) - 1;
    const Ground::Geo geo =
        Ground::TileFracToGeo({.X = static_cast<double>(sheet.Tile.X) + NodeFraction(sheet, i),
                               .Y = static_cast<double>(sheet.Tile.Y) + NodeFraction(sheet, j)},
                              sheet.Tile.Zoom);
    written[one] = Frame_
                       .Place({.LongitudeDeg = geo.LongitudeDeg,
                               .LatitudeDeg = geo.LatitudeDeg,
                               .HeightM = static_cast<double>(sheet.Nodes[where[one].second])})
                       .UpM;
  }
  const Heights heights{.WrittenM = written, .WasM = was};
  told.Pads = FloorsOf(yields, pressed, Stamp::Pad, at, heights);
  told.Corridors = FloorsOf(yields, pressed, Stamp::Corridor, at, heights);
  return told;
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
    if (!Live_->SetGroundGrid(fractions, error)) { return false; }
    GridPostings_ = sheet.Postings;
    return true;
  }
  return true;
}

std::array<float, 4> HeightSheets::StitchOf(const Sheet &sheet,
                                            const Patchwork &laid,
                                            std::span<const Data::TileId> present,
                                            int coarsest) {
  std::array<float, 4> stitched = {{}};
  if (sheet.Tile.Zoom <= coarsest) { return stitched; }
  MeasuresSeams(sheet, laid, stitched);
  const auto absent = [&present, &sheet](long dx, long dy) {
    const long x = static_cast<long>(sheet.Tile.X) + dx;
    const long y = static_cast<long>(sheet.Tile.Y) + dy;
    if (x < 0 || y < 0) { return true; }
    const Data::TileId asked{
        .Zoom = sheet.Tile.Zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
    return std::ranges::find(present, asked) == present.end();
  };
  for (size_t at = 0; at < kSeamEdges.size(); ++at) {
    if (stitched[at] > 0.5f && !absent(kSeamEdges[at].StepX, kSeamEdges[at].StepY)) {
      stitched[at] = 0.0f;
    }
  }
  return stitched;
}

bool HeightSheets::Hands(const Patchwork &laid, std::string &error) {
  if (Live_ == nullptr || !Framed_) { return true; }
  for (Held &one : Held_) { one.Wanted = false; }
  Flat_ = 0;
  Seams_ = {};
  Instances_.clear();
  Virtual_.clear();
  const size_t nodes = Render::GroundLattice::kPageNodes;
  std::vector<Data::TileId> present;
  present.reserve(laid.Sheets.size());
  int coarsest = std::numeric_limits<int>::max();
  for (const Sheet &sheet : laid.Sheets) {
    present.push_back(sheet.Tile);
    coarsest = std::min(coarsest, sheet.Tile.Zoom);
  }
  for (const Sheet &sheet : laid.Sheets) {
    Render::PageId page = Render::kNoPage;
    if (sheet.Side == Render::GroundLattice::kSide && sheet.Nodes.size() == nodes) {
      page = PageFor(sheet.Tile, sheet.Nodes, error);
    } else {
      ++Flat_;
      continue;
    }
    if (page == Render::kNoPage) { return false; }
    const std::array<float, 4> stitched = StitchOf(sheet, laid, present, coarsest);
    (sheet.Virtual ? Virtual_ : Instances_)
        .push_back(TileOf(sheet.Tile, page, sheet.Nodes, stitched));
  }
  if (!HandsGrid(laid, error)) { return false; }
  for (const Held &one : Held_) {
    if (!one.Wanted) { Live_->ReleaseHeightPage(one.Page); }
  }
  std::erase_if(Held_, [](const Held &one) { return !one.Wanted; });
  return Live_->SetGroundLattice(Instances_, Virtual_, error);
}

std::optional<double>
HeightSheets::FieldUpM(const Ground::GroundStream &ground, int zoom, EastNorth at) {
  const double eastM = at.EastM;
  const double southM = -at.NorthM;
  if (!Framed_) { return std::nullopt; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  Vec3 ecef;
  for (int axis = 0; axis < 3; ++axis) {
    ecef[axis] = origin[axis] + eastM * east[axis] - southM * north[axis];
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
  if (Live_ != nullptr) {
    for (const Held &one : Held_) { Live_->ReleaseHeightPage(one.Page); }
    std::string ignored;
    (void)Live_->SetGroundLattice({}, {}, ignored);
  }
  Held_.clear();
  Instances_.clear();
  Virtual_.clear();
  Fields_.clear();
  GridPostings_ = 0;
}

} // namespace outshine
