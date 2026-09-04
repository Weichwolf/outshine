#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Geodesy.h"
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

bool HeightSheets::Hands(const Patchwork &laid, std::string &error) {
  if (Live_ == nullptr || !Framed_) { return true; }
  for (Held &one : Held_) { one.Wanted = false; }
  Instances_.clear();
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
    Instances_.push_back(InstanceOf(sheet.Tile, page));
  }
  for (const Held &one : Held_) {
    if (!one.Wanted) { Live_->ReleaseHeightPage(one.Page); }
  }
  std::erase_if(Held_, [](const Held &one) { return !one.Wanted; });
  return Live_->SetGroundLattice(Instances_, error);
}

void HeightSheets::Clear() {
  if (Live_ != nullptr) {
    for (const Held &one : Held_) { Live_->ReleaseHeightPage(one.Page); }
    if (Zero_ != Render::kNoPage) { Live_->ReleaseHeightPage(Zero_); }
    std::string ignored;
    (void)Live_->SetGroundLattice({}, ignored);
  }
  Held_.clear();
  Instances_.clear();
  Zero_ = Render::kNoPage;
}

} // namespace outshine
