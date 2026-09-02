#include "Units.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <cmath>
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

GeoBounds TileBounds(int z, uint32_t x, uint32_t y) {
  GeoBounds b;
  const double n = std::ldexp(1.0, z);
  b.MinLonDeg = static_cast<double>(x) / n * kDegPerTurn - kDegPerHalfTurn;
  b.MaxLonDeg = static_cast<double>(x + 1) / n * kDegPerTurn - kDegPerHalfTurn;
  const double yn = 1.0 - 2.0 * static_cast<double>(y) / n;
  const double ys = 1.0 - 2.0 * static_cast<double>(y + 1) / n;
  b.MaxLatDeg = kRad2Deg * std::atan(std::sinh(kPi * yn));
  b.MinLatDeg = kRad2Deg * std::atan(std::sinh(kPi * ys));
  return b;
}

Geo TileLocalToGeo(int z, uint32_t x, uint32_t y, uint32_t extent, int32_t localX, int32_t localY) {
  Geo g;
  if (extent == 0) { return g; }

  const double n = std::ldexp(1.0, z);
  const double invExtent = 1.0 / static_cast<double>(extent);

  const double xf = static_cast<double>(x) + static_cast<double>(localX) * invExtent;
  const double yf = static_cast<double>(y) + static_cast<double>(localY) * invExtent;

  g.LongitudeDeg = xf / n * kDegPerTurn - kDegPerHalfTurn;
  const double yy = 1.0 - 2.0 * yf / n;
  g.LatitudeDeg = kRad2Deg * std::atan(std::sinh(kPi * yy));
  g.HeightM = 0.0;
  return g;
}

Geo TileFracToGeo(int z, uint32_t x, uint32_t y, double fx, double fy) {
  const double n = std::ldexp(1.0, z);
  const double xf = (static_cast<double>(x) + fx) / n;
  const double yf = (static_cast<double>(y) + fy) / n;

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
  const double originLatDeg = origin.LatitudeDeg;
  const double originLonDeg = origin.LongitudeDeg;
  if (originLatDeg < -kOriginMostLatDeg || originLatDeg > kOriginMostLatDeg) {
    return {State::OriginTooPolar, originLatDeg, originLonDeg, 0.0, 0.0};
  }

  const double mpd = kPi * kWgs84A / kDegPerHalfTurn;
  return {State::Usable, originLatDeg, originLonDeg, mpd, std::cos(originLatDeg * kDeg2Rad) * mpd};
}

TileEnuMap TileEnuMap::Over(const EnuFrame &frame, int z, uint32_t x, uint32_t y, uint32_t extent) {
  const GeoBounds b = TileBounds(z, x, y);
  Geo topLeft;
  topLeft.LongitudeDeg = b.MinLonDeg;
  topLeft.LatitudeDeg = b.MaxLatDeg;
  Geo bottomRight;
  bottomRight.LongitudeDeg = b.MaxLonDeg;
  bottomRight.LatitudeDeg = b.MinLatDeg;

  TileEnuMap map;
  Enu etl;
  Enu ebr;
  if (!frame.TryFromGeo(topLeft, &etl) || !frame.TryFromGeo(bottomRight, &ebr)) { return map; }

  map.OriginE_ = etl.EastM;
  map.OriginN_ = etl.NorthM;
  const double invExtent = (extent == 0) ? 0.0 : 1.0 / static_cast<double>(extent);
  map.ScaleE_ = (ebr.EastM - etl.EastM) * invExtent;
  map.ScaleN_ = (ebr.NorthM - etl.NorthM) * invExtent;
  map.Extent_ = extent;
  return map;
}

} // namespace outshine::Ground
