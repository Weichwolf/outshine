#include "TileGeodesy.h"

#include <cmath>

namespace outshine::World {

namespace {

constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 180.0 / kPi;

}  // namespace

TileIndex TileIndex::Of(Geo g, int z) {
  if (g.LatDeg < -kMercatorLatMaxDeg || g.LatDeg > kMercatorLatMaxDeg)
    return TileIndex(State::OutsideMercatorBand, 0, 0);

  const double n = std::ldexp(1.0, z);   /* 2^z exactly */
  const double xf = (g.LonDeg + 180.0) / 360.0 * n;
  const double yf = (1.0 - std::asinh(std::tan(g.LatDeg * kDeg2Rad)) / kPi) * 0.5 * n;

  /* For z=0, n=1, so the only legal tile is 0. */
  double xc = std::floor(xf);
  double yc = std::floor(yf);
  if (xc < 0) xc = 0;
  if (yc < 0) yc = 0;
  if (xc > n - 1) xc = n - 1;
  if (yc > n - 1) yc = n - 1;
  return TileIndex(State::Inside, (uint32_t)xc, (uint32_t)yc);
}

GeoBounds TileBounds(int z, uint32_t x, uint32_t y) {
  GeoBounds b;
  const double n = std::ldexp(1.0, z);
  b.MinLonDeg = (double)x / n * 360.0 - 180.0;
  b.MaxLonDeg = (double)(x + 1) / n * 360.0 - 180.0;
  const double yn = 1.0 - 2.0 * (double)y / n;
  const double ys = 1.0 - 2.0 * (double)(y + 1) / n;
  b.MaxLatDeg = kRad2Deg * std::atan(std::sinh(kPi * yn));
  b.MinLatDeg = kRad2Deg * std::atan(std::sinh(kPi * ys));
  return b;
}

Geo TileLocalToGeo(int z, uint32_t x, uint32_t y, uint32_t extent, int32_t localX, int32_t localY) {
  Geo g;
  if (extent == 0) return g;   /* caller bug; avoid DIV0 */

  const double n = std::ldexp(1.0, z);
  const double invExtent = 1.0 / (double)extent;

  /* localY grows south, matching the slippy-tile y convention directly, so this just adds. */
  const double xf = (double)x + (double)localX * invExtent;
  const double yf = (double)y + (double)localY * invExtent;

  g.LonDeg = xf / n * 360.0 - 180.0;
  const double yy = 1.0 - 2.0 * yf / n;
  g.LatDeg = kRad2Deg * std::atan(std::sinh(kPi * yy));
  g.AltM = 0.0;
  return g;
}

Geo TileFracToGeo(int z, uint32_t x, uint32_t y, double fx, double fy) {
  const double n = std::ldexp(1.0, z);   /* 2^z exactly */
  const double xf = ((double)x + fx) / n;
  const double yf = ((double)y + fy) / n;

  Geo g;
  g.LonDeg = xf * 360.0 - 180.0;
  const double yy = 1.0 - 2.0 * yf;
  g.LatDeg = kRad2Deg * std::atan(std::sinh(kPi * yy));
  g.AltM = 0.0;
  return g;
}

Ecef GeoToEcefWgs84(Geo g) {
  const double a = kWgs84A;
  const double f = kWgs84F;
  const double e2 = f * (2.0 - f);   /* first eccentricity squared */

  const double phi = g.LatDeg * kDeg2Rad;
  const double lam = g.LonDeg * kDeg2Rad;
  const double sphi = std::sin(phi), cphi = std::cos(phi);
  const double slam = std::sin(lam), clam = std::cos(lam);

  const double N = a / std::sqrt(1.0 - e2 * sphi * sphi);

  Ecef p;
  p.X = (N + g.AltM) * cphi * clam;
  p.Y = (N + g.AltM) * cphi * slam;
  p.Z = (N * (1.0 - e2) + g.AltM) * sphi;
  return p;
}

Geo EcefToGeoWgs84(Ecef p) {
  const double a = kWgs84A;
  const double f = kWgs84F;
  const double e2 = f * (2.0 - f);
  const double b = a * (1.0 - f);   /* semi-minor axis */

  const double pxy = std::sqrt(p.X * p.X + p.Y * p.Y);   /* distance from the spin axis */

  Geo g;

  /* On the spin axis longitude is undefined; define it as 0. */
  if (pxy < 1e-9) {
    g.LonDeg = 0.0;
    g.LatDeg = (p.Z >= 0.0) ? 90.0 : -90.0;
    g.AltM = std::fabs(p.Z) - b;
    return g;
  }

  g.LonDeg = kRad2Deg * std::atan2(p.Y, p.X);

  /* Bowring 1976, closed form via the parametric latitude; ep2 = second eccentricity squared. */
  const double ep2 = e2 / (1.0 - e2);
  const double theta = std::atan2(p.Z * a, pxy * b);
  const double st = std::sin(theta), ct = std::cos(theta);
  const double lat = std::atan2(p.Z + ep2 * b * st * st * st, pxy - e2 * a * ct * ct * ct);
  const double slat = std::sin(lat), clat = std::cos(lat);
  const double N = a / std::sqrt(1.0 - e2 * slat * slat);

  g.LatDeg = kRad2Deg * lat;
  g.AltM = pxy / clat - N;
  return g;
}

EnuFrame EnuFrame::At(double originLatDeg, double originLonDeg) {
  if (originLatDeg < -89.9 || originLatDeg > 89.9)
    return EnuFrame(State::OriginTooPolar, originLatDeg, originLonDeg, 0.0, 0.0);

  const double mpd = kPi * kEarthRadiusM / 180.0;   /* 111319.49... */
  return EnuFrame(State::Usable, originLatDeg, originLonDeg, mpd,
                  std::cos(originLatDeg * kDeg2Rad) * mpd);
}

TileEnuMap TileEnuMap::Over(const EnuFrame &frame, int z, uint32_t x, uint32_t y, uint32_t extent) {
  /* Two corners suffice: the affine map is defined by their ENU images. */
  const GeoBounds b = TileBounds(z, x, y);
  Geo topLeft;
  topLeft.LonDeg = b.MinLonDeg;
  topLeft.LatDeg = b.MaxLatDeg;
  Geo bottomRight;
  bottomRight.LonDeg = b.MaxLonDeg;
  bottomRight.LatDeg = b.MinLatDeg;

  TileEnuMap map;
  Enu etl, ebr;
  if (!frame.TryFromGeo(topLeft, &etl) || !frame.TryFromGeo(bottomRight, &ebr)) return map;

  map.OriginE_ = etl.E;
  map.OriginN_ = etl.N;
  const double invExtent = (extent == 0) ? 0.0 : 1.0 / (double)extent;
  map.ScaleE_ = (ebr.E - etl.E) * invExtent;
  map.ScaleN_ = (ebr.N - etl.N) * invExtent;
  map.Extent_ = extent;
  return map;
}

}  // namespace outshine::World
