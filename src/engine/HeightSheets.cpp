#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
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

} // namespace

Render::PageId
HeightSheets::PageFor(Data::TileId tile, std::span<const float> nodes, std::string &error) {
  for (Held &one : Held_) {
    if (one.Tile == tile) {
      one.Wanted = true;
      return one.Page;
    }
  }
  const Render::PageId page = Live_->PlaceHeightPage(nodes, error);
  if (page == Render::kNoPage) { return page; }
  Held_.push_back({.Tile = tile, .Page = page, .Wanted = true});
  return page;
}

Render::GroundInstance HeightSheets::InstanceOf(Data::TileId tile, Render::PageId page) const {
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
    made.Row[column * 4u + 2u] = static_cast<float>(-Dot(north, *axes[column]));
    made.Row[column * 4u + 3u] = 0.0f;
  }
  made.Row[12] = static_cast<float>(at.EastM);
  made.Row[13] = static_cast<float>(at.UpM);
  made.Row[14] = static_cast<float>(-at.NorthM);
  made.Row[15] = 1.0f;
  made.Corners = {{nw[0], nw[1], ne[0], ne[1], sw[0], sw[1], se[0], se[1]}};
  made.Page = static_cast<float>(page);
  made.SagInv = static_cast<float>(1.0 / std::sqrt(Dot(centre, centre)));
  const auto steps = static_cast<float>(Render::GroundLattice::kSide - 1);
  made.StepE = 0.5f * ((ne[0] - nw[0]) + (se[0] - sw[0])) / steps;
  made.StepN = 0.5f * ((nw[1] - sw[1]) + (ne[1] - se[1])) / steps;
  return made;
}

namespace {

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
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

void SampleVirtual(const Ground::TerrainField &field,
                   Data::TileId tile,
                   uint32_t drop,
                   Sheet &sheet) {
  const int side = Render::GroundLattice::kSide;
  const double span = 1.0 / static_cast<double>(1u << drop);
  const double offX = static_cast<double>(tile.X & ((1u << drop) - 1u)) * span;
  const double offY = static_cast<double>(tile.Y & ((1u << drop) - 1u)) * span;
  sheet.Nodes.resize(Render::GroundLattice::kNodes);
  for (int j = 0; j < side; ++j) {
    const double fy = offY + span * static_cast<double>(j) / static_cast<double>(side - 1);
    for (int i = 0; i < side; ++i) {
      const double fx = offX + span * static_cast<double>(i) / static_cast<double>(side - 1);
      sheet.Nodes[static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i)] =
          field.PostingM({.Col = fx, .Row = fy});
    }
  }
}

} // namespace

size_t HeightSheets::Refine(Patchwork &laid, const Ground::GroundStream &ground, Nearer how) {
  if (how.Levels <= 0) { return 0; }
  std::vector<std::pair<Data::TileId, Ground::TerrainGrid>> fields;
  const auto fieldOf = [&](Data::TileId parent) -> const Ground::TerrainField * {
    for (const auto &one : fields) {
      if (one.first == parent) { return one.second.TryField(); }
    }
    fields.emplace_back(parent, ground.FieldOf(parent));
    return fields.back().second.TryField();
  };
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
        const auto drop = static_cast<uint32_t>(level);
        const Data::TileId tile{
            .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
        const Ground::TerrainField *field =
            fieldOf({.Zoom = how.FinestZoom, .X = tile.X >> drop, .Y = tile.Y >> drop});
        Sheet sheet{
            .Tile = tile, .Side = Render::GroundLattice::kSide, .Postings = 0, .Virtual = true};
        if (field != nullptr && field->Meshable()) { SampleVirtual(*field, tile, drop, sheet); }
        made.push_back(std::move(sheet));
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

size_t HeightSheets::Press(std::span<const Yields> yields, Patchwork &laid) const {
  if (!Framed_ || yields.empty()) { return 0; }
  const int side = Render::GroundLattice::kSide;
  std::vector<EastSouth> at;
  std::vector<double> up;
  std::vector<std::pair<size_t, size_t>> where;
  for (size_t sheet = 0; sheet < laid.Sheets.size(); ++sheet) {
    const Sheet &one = laid.Sheets[sheet];
    if (one.Side != side || (!one.Virtual && one.Postings < 2) ||
        one.Nodes.size() != Render::GroundLattice::kNodes) {
      continue;
    }
    const auto fractionOf = [&one](int k) {
      constexpr int side = Render::GroundLattice::kSide;
      return one.Virtual ? static_cast<double>(k) / static_cast<double>(side - 1)
                         : FractionOf(k, one.Postings, side);
    };
    for (int j = 0; j < side; ++j) {
      const double fy = fractionOf(j);
      for (int i = 0; i < side; ++i) {
        const double fx = fractionOf(i);
        const Ground::Geo geo = Ground::TileFracToGeo(
            {.X = static_cast<double>(one.Tile.X) + fx, .Y = static_cast<double>(one.Tile.Y) + fy},
            one.Tile.Zoom);
        const size_t node =
            static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i);
        const EastNorthUp stood = Frame_.Place({.LongitudeDeg = geo.LongitudeDeg,
                                                .LatitudeDeg = geo.LatitudeDeg,
                                                .HeightM = static_cast<double>(one.Nodes[node])});
        at.push_back({.EastM = stood.EastM, .SouthM = -stood.NorthM});
        up.push_back(stood.UpM);
        where.emplace_back(sheet, node);
      }
    }
  }
  std::vector<double> was(up);
  const size_t moved = PressPoints(yields, at, up);
  if (moved == 0) { return 0; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  const Vec3 &upward = Frame_.UpEcef();
  for (size_t one = 0; one < up.size(); ++one) {
    if (up[one] == was[one]) { continue; }
    const double e = at[one].EastM;
    const double n = -at[one].SouthM;
    Vec3 ecef;
    for (int axis = 0; axis < 3; ++axis) {
      ecef[axis] = origin[axis] + e * east[axis] + n * north[axis] + up[one] * upward[axis];
    }
    const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
    laid.Sheets[where[one].first].Nodes[where[one].second] = static_cast<float>(geo.HeightM);
  }
  return moved;
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

bool HeightSheets::Hands(const Patchwork &laid, std::string &error) {
  if (Live_ == nullptr || !Framed_) { return true; }
  for (Held &one : Held_) { one.Wanted = false; }
  Instances_.clear();
  Virtual_.clear();
  const size_t nodes = Render::GroundLattice::kNodes;
  for (const Sheet &sheet : laid.Sheets) {
    Render::PageId page = Render::kNoPage;
    if (sheet.Side == Render::GroundLattice::kSide && sheet.Nodes.size() == nodes) {
      page = PageFor(sheet.Tile, sheet.Nodes, error);
    } else {
      if (Zero_ == Render::kNoPage) {
        const std::vector<float> flat(nodes, 0.0f);
        Zero_ = Live_->PlaceHeightPage(flat, error);
      }
      page = Zero_;
    }
    if (page == Render::kNoPage) { return false; }
    (sheet.Virtual ? Virtual_ : Instances_).push_back(InstanceOf(sheet.Tile, page));
  }
  if (!HandsGrid(laid, error)) { return false; }
  for (const Held &one : Held_) {
    if (!one.Wanted) { Live_->ReleaseHeightPage(one.Page); }
  }
  std::erase_if(Held_, [](const Held &one) { return !one.Wanted; });
  return Live_->SetGroundLattice(Instances_, Virtual_, error);
}

std::optional<double>
HeightSheets::FieldUpM(const Ground::GroundStream &ground, int zoom, EastSouth at) {
  const double eastM = at.EastM;
  const double southM = at.SouthM;
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
  const Ground::TerrainField *field = nullptr;
  for (const auto &one : Fields_) {
    if (one.first == tile) { field = one.second.TryField(); }
  }
  if (field == nullptr) {
    Fields_.emplace_back(tile, ground.FieldOf(tile));
    field = Fields_.back().second.TryField();
  }
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
    if (Zero_ != Render::kNoPage) { Live_->ReleaseHeightPage(Zero_); }
    std::string ignored;
    (void)Live_->SetGroundLattice({}, {}, ignored);
  }
  Held_.clear();
  Instances_.clear();
  Virtual_.clear();
  Fields_.clear();
  Zero_ = Render::kNoPage;
  GridPostings_ = 0;
}

} // namespace outshine
