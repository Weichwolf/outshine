#include "Tile.h"

#include <cmath>

#include "Geodesy.h"
#include "Units.h"

namespace outshine::Generators {

namespace {

uint64_t Mix(uint64_t v) {
  v += 0x9e3779b97f4a7c15ull;
  v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
  v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
  return v ^ (v >> 31);
}

double TileLatDeg(int y, int zoom) {
  const double n = kPi - 2.0 * kPi * (double)y / (double)(1u << (unsigned)zoom);
  return kRad2Deg * std::atan(std::sinh(n));
}

double TileLonDeg(int x, int zoom) {
  return (double)x / (double)(1u << (unsigned)zoom) * 360.0 - 180.0;
}

} // namespace

Tile::Tile(int zoom, int x, int y) : Zoom_(zoom), X_(x), Y_(y) {
  Seed_ = Mix(((uint64_t)zoom << 58) ^ ((uint64_t)(uint32_t)x << 29) ^ (uint64_t)(uint32_t)y);
  const double south = TileLatDeg(y + 1, zoom), north = TileLatDeg(y, zoom);
  const double west = TileLonDeg(x, zoom), east = TileLonDeg(x + 1, zoom);
  AnchorLat_ = south;
  AnchorLon_ = west;
  SpanNm_ = (north - south) * kMPerDeg;
  SpanEm_ = (east - west) * kMPerDeg * std::cos(0.5 * (north + south) * kDeg2Rad);
}

Tile Tile::Of(int zoom, double lat, double lon) {
  const double scale = (double)(1u << (unsigned)zoom);
  const double s = std::sin(lat * kDeg2Rad);
  const int x = (int)std::floor((lon + 180.0) / 360.0 * scale);
  const int y = (int)std::floor((0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * kPi)) * scale);
  return Tile(zoom, x, y);
}

uint64_t Tile::Seed(uint64_t stream) const {
  return Mix(Seed_ ^ Mix(stream));
}

void Tile::Enu(double lat, double lon, double *eastM, double *northM) const {
  *northM = (lat - AnchorLat_) * kMPerDeg;
  *eastM = Wrap180(lon - AnchorLon_) * kMPerDeg * std::cos(lat * kDeg2Rad);
}

void Tile::Geo(double eastM, double northM, double *lat, double *lon) const {
  const double atLat = AnchorLat_ + northM / kMPerDeg;
  *lat = atLat;
  *lon = AnchorLon_ + eastM / (kMPerDeg * std::cos(atLat * kDeg2Rad));
}

void Tile::AnchorEcef(double aslM, double out[3]) const {
  GeoToEcef(AnchorLat_, AnchorLon_, aslM, out);
}

} // namespace outshine::Generators
