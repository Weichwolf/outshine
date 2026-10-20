#include "math/Units.h"
#include "TileGeodesy.h"
#include "Geocentric.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <limits>
#include <cstdint>

namespace outshine::Ground {

constexpr double kPoleLatDeg = 90.0;
constexpr double kOriginMostLatDeg = 89.9;

namespace {

constexpr double kDeg2Rad = kPi / kDegPerHalfTurn;
constexpr double kRad2Deg = kDegPerHalfTurn / kPi;

[[nodiscard]] double InverseMercatorLatitude(double y) {
  static const double poleDistance =
      std::log(4.0 / std::numeric_limits<double>::epsilon()) / (2.0 * kPi);
  const double centred = 0.5 - y;
  if (std::abs(centred) >= poleDistance) { return std::copysign(kPoleLatDeg, centred); }
  return kRad2Deg * std::atan(std::sinh(kPi * (1.0 - 2.0 * y)));
}

}

TileIndex TileIndex::Of(Geo g, int z) noexcept {
  if (!std::isfinite(g.LatitudeDeg) || !std::isfinite(g.LongitudeDeg) ||
      std::abs(g.LatitudeDeg) > kPoleLatDeg || std::abs(g.LongitudeDeg) > kDegPerHalfTurn ||
      z < 0 || z > std::numeric_limits<uint32_t>::digits) {
    return {State::InvalidInput, Data::TileId{}};
  }
  if (g.LatitudeDeg < -kMercatorLatMaxDeg || g.LatitudeDeg > kMercatorLatMaxDeg) {
    return {State::OutsideMercatorBand, Data::TileId{}};
  }

  const double n = std::ldexp(1.0, z);
  const double xf = (g.LongitudeDeg + kDegPerHalfTurn) / kDegPerTurn * n;
  const double yf = (1.0 - std::asinh(std::tan(g.LatitudeDeg * kDeg2Rad)) / kPi) * 0.5 * n;

  double xc = std::floor(xf);
  double yc = std::floor(yf);
  xc = std::max<double>(xc, 0);
  yc = std::max<double>(yc, 0);
  xc = std::min(xc, n - 1);
  yc = std::min(yc, n - 1);
  return {State::Inside,
          Data::TileId{.Zoom = z, .X = static_cast<uint32_t>(xc), .Y = static_cast<uint32_t>(yc)}};
}

GeoBounds TileBounds(Data::TileId of) {
  GeoBounds b;
  const double n = std::ldexp(1.0, of.Zoom);
  b.MinLonDeg = static_cast<double>(of.X) / n * kDegPerTurn - kDegPerHalfTurn;
  b.MaxLonDeg = (static_cast<double>(of.X) + 1.0) / n * kDegPerTurn - kDegPerHalfTurn;
  const double yn = 1.0 - 2.0 * static_cast<double>(of.Y) / n;
  const double ys = 1.0 - 2.0 * (static_cast<double>(of.Y) + 1.0) / n;
  b.MaxLatDeg = kRad2Deg * std::atan(std::sinh(kPi * yn));
  b.MinLatDeg = kRad2Deg * std::atan(std::sinh(kPi * ys));
  return b;
}

Geo TileFracToGeo(TileFrac at, int z) {
  const double n = std::ldexp(1.0, z);
  const double xf = at.X / n;
  const double yf = at.Y / n;

  Geo g;
  g.LongitudeDeg = xf * kDegPerTurn - kDegPerHalfTurn;
  g.LatitudeDeg = InverseMercatorLatitude(yf);
  g.HeightM = 0.0;
  return g;
}

Ecef GeoToEcefWgs84(Geo g) {
  Vec3 point;
  GeoToEcef(g, point);
  return {.X = point[0], .Y = point[1], .Z = point[2]};
}

Geo EcefToGeoWgs84(Ecef p) {
  constexpr double invalid = std::numeric_limits<double>::quiet_NaN();
  return GeoFromEcef(Vec3{{p.X, p.Y, p.Z}})
      .value_or(Geo{.LongitudeDeg = invalid, .LatitudeDeg = invalid, .HeightM = invalid});
}

EnuFrame EnuFrame::At(Geo origin) {
  const LongitudeLatitude stands{.LongitudeDeg = origin.LongitudeDeg,
                                 .LatitudeDeg = origin.LatitudeDeg};
  if (stands.LatitudeDeg < -kOriginMostLatDeg || stands.LatitudeDeg > kOriginMostLatDeg) {
    return {State::OriginTooPolar, stands, {}};
  }

  const double mpd = kPi * kWgs84A / kDegPerHalfTurn;
  return {State::Usable,
          stands,
          {.Longitude = std::cos(stands.LatitudeDeg * kDeg2Rad) * mpd, .Latitude = mpd}};
}

TileEnuMap TileEnuMap::Over(const EnuFrame &frame, Data::TileId of, uint32_t extent) {
  const GeoBounds b = TileBounds(of);
  Geo topLeft;
  topLeft.LongitudeDeg = b.MinLonDeg;
  topLeft.LatitudeDeg = b.MaxLatDeg;
  Geo bottomRight;
  bottomRight.LongitudeDeg = b.MaxLonDeg;
  bottomRight.LatitudeDeg = b.MinLatDeg;

  TileEnuMap map;
  const std::optional<Enu> etl = frame.FromGeo(topLeft);
  const std::optional<Enu> ebr = frame.FromGeo(bottomRight);
  if (!etl || !ebr) { return map; }

  map.OriginE_ = etl->EastM;
  map.OriginN_ = etl->NorthM;
  const double invExtent = (extent == 0) ? 0.0 : 1.0 / static_cast<double>(extent);
  map.ScaleE_ = (ebr->EastM - etl->EastM) * invExtent;
  map.ScaleN_ = (ebr->NorthM - etl->NorthM) * invExtent;
  map.Extent_ = extent;
  return map;
}

}
