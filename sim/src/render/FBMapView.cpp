#include "FBMapView.h"

#include <cmath>

namespace FlightBox::Render {

namespace {
constexpr double kPiL = 3.14159265358979323846;
constexpr double kDeg = kPiL / 180.0;
/* atan(sinh(pi)) — the latitude where Mercator y reaches 0 and 1. */
constexpr double kMercLatLimit = 85.05112877980659;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

double FBMercX(double lonDeg) {
  while (lonDeg > 180.0) lonDeg -= 360.0;
  while (lonDeg < -180.0) lonDeg += 360.0;
  return (lonDeg + 180.0) / 360.0;
}

double FBMercY(double latDeg) {
  double lat = Clamp(latDeg, -kMercLatLimit, kMercLatLimit) * kDeg;
  return 0.5 - std::asinh(std::tan(lat)) / (2.0 * kPiL);
}

double FBMercLon(double x) { return x * 360.0 - 180.0; }

double FBMercLat(double y) { return std::atan(std::sinh((0.5 - y) * 2.0 * kPiL)) / kDeg; }

double FBMapView::PxPerMerc() const {
  if (Width <= 0 || SpanM <= 1.0) return 1.0;
  double cosLat = std::cos(Clamp(CentreLatDeg, -kMercLatLimit, kMercLatLimit) * kDeg);
  if (cosLat < 1e-6) cosLat = 1e-6;
  return (double)Width / SpanM * kMercEquatorM * cosLat;
}

int FBMapView::SheetZoom(int ts) const {
  if (ts <= 0) return kMapZoomMin;
  /* A tile at zoom z covers 1/2^z Mercator units, i.e. PxPerMerc/2^z pixels; == ts is 1 texel/pixel. */
  double z = std::log2(PxPerMerc() / (double)ts);
  int zi = (int)std::lround(z);
  return zi < kMapZoomMin ? kMapZoomMin : (zi > kMapZoomMax ? kMapZoomMax : zi);
}

void FBMapView::Project(double latDeg, double lonDeg, float &x, float &y) const {
  double p = PxPerMerc();
  double dx = FBMercX(lonDeg) - FBMercX(CentreLonDeg);
  if (dx > 0.5) dx -= 1.0;      /* the short way round: a map may straddle the antimeridian */
  if (dx < -0.5) dx += 1.0;
  x = (float)((double)CentreX() + dx * p);
  y = (float)((double)CentreY() + (FBMercY(latDeg) - FBMercY(CentreLatDeg)) * p);
}

void FBMapView::Unproject(float x, float y, double &latDeg, double &lonDeg) const {
  double p = PxPerMerc();
  if (p < 1e-9) p = 1e-9;
  lonDeg = FBMercLon(FBMercX(CentreLonDeg) + ((double)x - (double)CentreX()) / p);
  latDeg = FBMercLat(FBMercY(CentreLatDeg) + ((double)y - (double)CentreY()) / p);
}

double FBMapSpanForZoom(int zoom, int widthPx, int ts, double centreLatDeg) {
  if (widthPx <= 0 || ts <= 0) return 1.0;
  double cosLat = std::cos(Clamp(centreLatDeg, -kMercLatLimit, kMercLatLimit) * kDeg);
  if (cosLat < 1e-6) cosLat = 1e-6;
  double pxPerMerc = (double)ts * std::exp2((double)zoom);
  return (double)widthPx / pxPerMerc * kMercEquatorM * cosLat;
}

} // namespace FlightBox::Render
