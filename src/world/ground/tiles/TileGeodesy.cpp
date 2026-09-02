#include "Units.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <cstdint>

namespace outshine::Ground {

constexpr double kAtPoleXy = 1e-9;
constexpr double kPoleLatDeg = 90.0;
constexpr double kOriginMostLatDeg = 89.9;

namespace {

constexpr double kDeg2Rad = kPi / kDegPerHalfTurn;
constexpr double kRad2Deg = kDegPerHalfTurn / kPi;

} // namespace

TileIndex TileIndex::Of(Geo g, int z) {
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
  b.MaxLonDeg = static_cast<double>(of.X + 1) / n * kDegPerTurn - kDegPerHalfTurn;
  const double yn = 1.0 - 2.0 * static_cast<double>(of.Y) / n;
  const double ys = 1.0 - 2.0 * static_cast<double>(of.Y + 1) / n;
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
  const double yy = 1.0 - 2.0 * yf;
  g.LatitudeDeg = kRad2Deg * std::atan(std::sinh(kPi * yy));
  g.HeightM = 0.0;
  return g;
}

Ecef GeoToEcefWgs84(Geo g) {
  const double a = kWgs84A;
  const double f = kWgs84F;
  const double e2 = f * (2.0 - f);

  const double phi = g.LatitudeDeg * kDeg2Rad;
  const double lam = g.LongitudeDeg * kDeg2Rad;
  const double sphi = std::sin(phi);
  const double cphi = std::cos(phi);
  const double slam = std::sin(lam);
  const double clam = std::cos(lam);

  const double N = a / std::sqrt(1.0 - e2 * sphi * sphi);

  Ecef p;
  p.X = (N + g.HeightM) * cphi * clam;
  p.Y = (N + g.HeightM) * cphi * slam;
  p.Z = (N * (1.0 - e2) + g.HeightM) * sphi;
  return p;
}

Geo EcefToGeoWgs84(Ecef p) {
  const double a = kWgs84A;
  const double f = kWgs84F;
  const double e2 = f * (2.0 - f);
  const double b = a * (1.0 - f);

  const double pxy = std::sqrt(p.X * p.X + p.Y * p.Y);

  Geo g;

  if (pxy < kAtPoleXy) {
    g.LongitudeDeg = 0.0;
    g.LatitudeDeg = (p.Z >= 0.0) ? kPoleLatDeg : -kPoleLatDeg;
    g.HeightM = std::fabs(p.Z) - b;
    return g;
  }

  g.LongitudeDeg = kRad2Deg * std::atan2(p.Y, p.X);

  const double ep2 = e2 / (1.0 - e2);
  const double theta = std::atan2(p.Z * a, pxy * b);
  const double st = std::sin(theta);
  const double ct = std::cos(theta);
  const double lat = std::atan2(p.Z + ep2 * b * st * st * st, pxy - e2 * a * ct * ct * ct);
  const double slat = std::sin(lat);
  const double clat = std::cos(lat);
  const double N = a / std::sqrt(1.0 - e2 * slat * slat);

  g.LatitudeDeg = kRad2Deg * lat;
  g.HeightM = pxy / clat - N;
  return g;
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

} // namespace outshine::Ground
