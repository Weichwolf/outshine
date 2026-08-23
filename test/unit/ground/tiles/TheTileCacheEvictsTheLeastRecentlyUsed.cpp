#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"
#include "TerrariumPng.h"

#include "TerrainTiles.h"

using outshine::Ground::Ecef;
using outshine::Ground::EcefToGeoWgs84;
using outshine::Ground::EnuFrame;
using outshine::Ground::Geo;
using outshine::Ground::GeoToEcefWgs84;
using outshine::Ground::TerrainBytes;
using outshine::Ground::TerrainSource;
using outshine::Ground::TerrainTiles;

namespace {

constexpr uint32_t kSide = 5;

// a source that COUNTS what it is asked for: a cache hit is an ask that never happened
class Counting : public TerrainSource {
public:
  std::vector<std::pair<uint32_t, uint32_t>> Asked;
  [[nodiscard]] TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    Asked.push_back({x, y});
    std::vector<float> metres((size_t)kSide * kSide, (float)(x * 100u + y));
    return TerrainBytes::From(z, x, y, outshine::Test::TerrariumPng(kSide, kSide, metres));
  }
  [[nodiscard]] size_t AsksFor(uint32_t x, uint32_t y) const {
    size_t count = 0;
    for (const auto &one : Asked) {
      if (one.first == x && one.second == y) { ++count; }
    }
    return count;
  }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const EnuFrame frame = EnuFrame::At(48.1, 11.5);
  CHECK(frame.Where() == EnuFrame::State::Usable, "the frame stands");

  {
    // one stitched tile touches five raw tiles (itself and four neighbours), so a cache
    // that holds them answers the SECOND ask entirely from residence
    Counting source;
    TerrainTiles::Config config;
    config.DemCacheTiles = 16;
    TerrainTiles tiles(source, frame, config);
    (void)tiles.StitchedGrid(12, 100, 200);
    const size_t afterFirst = source.Asked.size();
    (void)tiles.StitchedGrid(12, 100, 200);
    CHECK(source.Asked.size() == afterFirst,
          "**A CACHED TILE IS NOT FETCHED AGAIN**: the second stitch of one tile reached "
          "the source not once (board:1757)");
    Note("raw tiles the first stitch fetched", (double)afterFirst, "tiles");
    Note("the cache holds", (double)tiles.HeapBytes() / 1024.0, "KiB");
    CHECK(tiles.HeapBytes() > (size_t)kSide * kSide * sizeof(float),
          "and HeapBytes tracks what is held rather than answering a constant");
  }
  {
    // the victim is the LEAST RECENTLY USED: one stitch touches nine raw tiles (itself,
    // four edges, four diagonals), so a cache of sixteen holds one stitch and part of the
    // next -- walk far enough and the first stitch's tiles must give way, while a tile
    // touched on the way must not
    Counting source;
    TerrainTiles::Config config;
    config.DemCacheTiles = 16;
    TerrainTiles tiles(source, frame, config);
    (void)tiles.StitchedGrid(12, 100, 200);
    const size_t askedFirst = source.AsksFor(100, 200);
    CHECK(askedFirst == 1, "the first stitch fetched its own tile once");

    for (uint32_t x = 140; x < 148; ++x) { (void)tiles.StitchedGrid(12, x, 300); }
    const uint32_t recent = 147;
    const size_t beforeRecent = source.AsksFor(recent, 300);
    (void)tiles.StitchedGrid(12, recent, 300);
    CHECK(source.AsksFor(recent, 300) == beforeRecent,
          "**THE TILE JUST TOUCHED IS STILL RESIDENT** -- a re-store of a live key does "
          "not duplicate or evict it");

    (void)tiles.StitchedGrid(12, 100, 200);
    CHECK(source.AsksFor(100, 200) > askedFirst,
          "**AND THE VICTIM WAS THE LEAST RECENTLY USED**: the tile nothing had touched "
          "through eight stitches elsewhere is the one that gave way (board:1757)");
  }

  {
    // the round trip, to the pole: the altitude has two forms and each divides by a
    // quantity that vanishes where the other is used (board:1757)
    double worstLat = 0.0, worstAlt = 0.0, at = 0.0;
    for (const double latDeg : {-89.9999999, -89.99999, -89.999, -45.0, -0.0001, 0.0,
                               0.0001, 45.0, 89.999, 89.99999, 89.9999999}) {
      for (const double altM : {-400.0, 0.0, 8848.0}) {
        const Geo asked{11.5, latDeg, altM};
        const Geo back = EcefToGeoWgs84(GeoToEcefWgs84(asked));
        if (std::fabs(back.LatDeg - latDeg) > worstLat) { worstLat = std::fabs(back.LatDeg - latDeg); }
        if (std::fabs(back.AltM - altM) > worstAlt) {
          worstAlt = std::fabs(back.AltM - altM);
          at = latDeg;
        }
      }
    }
    Note("the worst latitude error over the sweep", worstLat, "degrees");
    Note("the worst altitude error over the sweep", worstAlt, "m");
    std::printf("NOTE worst altitude at latitude %.3f\n", at);
    CHECK(worstLat < 1.0e-9,
          "**THE WGS84 ROUND TRIP HOLDS TO THE POLE**: latitude returns within a "
          "nanodegree from -89.999 to 89.999");
    CHECK(worstAlt < 1.0e-6,
          "**AND SO DOES THE ALTITUDE**: a micrometre over the same sweep -- the form that "
          "divides by cos(lat) is not the one asked near the pole (board:1757)");
  }

  Covers("I.29 the tile cache evicts the least recently used and the geodesy holds to the "
         "pole: the two behaviours board:1745 named and left with nobody (board:1757)");
  return Report();
}
