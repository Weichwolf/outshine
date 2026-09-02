#include "Tile.h"
#include "math/Vec3.h"

#include <cmath>
#include <cstdint>

#include "Geodesy.h"
#include "Units.h"

namespace outshine::Generators {

constexpr uint64_t kGoldenWord = 0x9e3779b97f4a7c15ull;
constexpr uint64_t kSplitMixFirst = 0xbf58476d1ce4e5b9ull;
constexpr uint64_t kSplitMixSecond = 0x94d049bb133111ebull;
constexpr unsigned kSplitMixShiftA = 30u;
constexpr unsigned kSplitMixShiftB = 27u;
constexpr unsigned kSplitMixShiftC = 31u;
constexpr unsigned kZoomShift = 58u;
constexpr unsigned kColumnShift = 29u;

namespace {

uint64_t Mix(uint64_t v) {
  v += kGoldenWord;
  v = (v ^ (v >> kSplitMixShiftA)) * kSplitMixFirst;
  v = (v ^ (v >> kSplitMixShiftB)) * kSplitMixSecond;
  return v ^ (v >> kSplitMixShiftC);
}

double TileLatDeg(int y, int zoom) {
  const double n = kPi - 2.0 * kPi * static_cast<double>(y) /
                             static_cast<double>(1u << static_cast<unsigned>(zoom));
  return kRad2Deg * std::atan(std::sinh(n));
}

double TileLonDeg(int x, int zoom) {
  return static_cast<double>(x) / static_cast<double>(1u << static_cast<unsigned>(zoom)) *
             kDegPerTurn -
         kDegPerHalfTurn;
}

} // namespace

Tile::Tile(int zoom, int x, int y) : Zoom_(zoom), X_(x), Y_(y) {
  Seed_ = Mix((static_cast<uint64_t>(zoom) << kZoomShift) ^
              (static_cast<uint64_t>(static_cast<uint32_t>(x)) << kColumnShift) ^
              static_cast<uint64_t>(static_cast<uint32_t>(y)));
  const double south = TileLatDeg(y + 1, zoom);
  const double north = TileLatDeg(y, zoom);
  const double west = TileLonDeg(x, zoom);
  const double east = TileLonDeg(x + 1, zoom);
  AnchorLat_ = south;
  AnchorLon_ = west;
  SpanNm_ = (north - south) * kMPerDeg;
  SpanEm_ = (east - west) * kMPerDeg * std::cos(0.5 * (north + south) * kDeg2Rad);
}

Tile Tile::Of(int zoom, double lat, double lon) {
  const auto scale = static_cast<double>(1u << static_cast<unsigned>(zoom));
  const double s = std::sin(lat * kDeg2Rad);
  const int x = static_cast<int>(std::floor((lon + kDegPerHalfTurn) / kDegPerTurn * scale));
  const int y =
      static_cast<int>(std::floor((0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * kPi)) * scale));
  return {zoom, x, y};
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

void Tile::AnchorEcef(double aslM, Vec3 &out) const {
  GeoToEcef(AnchorLat_, AnchorLon_, aslM, out);
}

} // namespace outshine::Generators
