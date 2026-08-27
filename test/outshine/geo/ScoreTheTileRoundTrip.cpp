#include <cmath>
#include <cstdio>

#include "Check.h"
#include "TileGeodesy.h"

namespace {

// The oracle is IDENTITY, and it does not depend on our design: a coordinate turned into a tile
// and turned back is the coordinate it started as, to within the resolution the tile has. There
// is nothing to believe here -- either the two directions compose to nothing, or one of them is
// wrong.
//
// This case exists because a single positional initialiser sent the ground somewhere else and
// nothing noticed for as long as the branch behind it was nailed shut:
//
//   Ground::Geo{over.LatDeg, over.LonDeg}   against   struct Geo { LonDeg, LatDeg, AltM }
//
// It reads correctly and means the opposite. Munich at 48.14 N, 11.58 E became 11.58 N,
// 48.14 E -- the Indian Ocean off Somalia -- and the terrain source answered, because a
// terrain source answers everywhere. The tiles meshed, the ring stood, and the ground was
// 2712 km away at an altitude of zero.
//
// So the round trip is checked at coordinates that are NOT symmetric under a swap, which is
// what makes the swap visible: a place where latitude and longitude are far apart, and a place
// where their signs differ.
constexpr double kWithinDeg = 0.02;

struct Where {
  const char *What;
  double LatDeg, LonDeg;
};

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Ground;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Where asked[] = {
      {"Munich, where the driver drives", 48.1394, 11.5757},
      {"Sydney, southern and eastern", -33.8688, 151.2093},
      {"Quito, on the equator and west", -0.1807, -78.4678},
      {"Anchorage, far north and far west", 61.2181, -149.9003},
  };

  double worstLat = 0.0, worstLon = 0.0;
  const char *worstAt = "nowhere";
  for (const Where &one : asked) {
    for (const int zoom : {10, 14, 16}) {
      const TileFrac at = ToTileFracClamped(Geo{.LonDeg = one.LonDeg, .LatDeg = one.LatDeg}, zoom);
      const uint32_t x = (uint32_t)at.X;
      const uint32_t y = (uint32_t)at.Y;
      const Geo back = TileFracToGeo(zoom, x, y, at.X - (double)x, at.Y - (double)y);
      const double offLat = std::fabs(back.LatDeg - one.LatDeg);
      const double offLon = std::fabs(back.LonDeg - one.LonDeg);
      if (offLat > worstLat) { worstLat = offLat; worstAt = one.What; }
      if (offLon > worstLon) { worstLon = offLon; }
      if (zoom == 14) {
        std::printf("%-38s asked %+9.4f,%+10.4f  came back %+9.4f,%+10.4f\n", one.What,
                    one.LatDeg, one.LonDeg, back.LatDeg, back.LonDeg);
      }
    }
  }

  std::printf("THE WORST OF THEM was %s\n", worstAt);

  Note("the worst the latitude moved", worstLat, "deg");
  Note("the worst the longitude moved", worstLon, "deg");
  CHECK(worstLat < kWithinDeg && worstLon < kWithinDeg,
        "**A COORDINATE TURNED INTO A TILE AND BACK IS WHERE IT STARTED**: the two directions "
        "compose to nothing, or one of them is wrong -- and a place whose latitude and longitude "
        "are far apart is where a swapped pair stops being invisible");

  // The control, and it is the defect this case was written against: the very same round trip
  // with the pair given the other way round lands thousands of kilometres away, so this case
  // can tell a working conversion from one that is merely self-consistent.
  double worstSwapped = 0.0;
  for (const Where &one : asked) {
    const TileFrac at = ToTileFracClamped(Geo{.LonDeg = one.LatDeg, .LatDeg = one.LonDeg}, 14);
    const uint32_t x = (uint32_t)at.X;
    const uint32_t y = (uint32_t)at.Y;
    const Geo back = TileFracToGeo(14, x, y, at.X - (double)x, at.Y - (double)y);
    const double away = std::fabs(back.LatDeg - one.LatDeg) + std::fabs(back.LonDeg - one.LonDeg);
    if (away > worstSwapped) { worstSwapped = away; }
  }
  Note("how far the same round trip lands with the pair swapped", worstSwapped, "deg");
  CHECK(worstSwapped > 30.0,
        "and the control is a control: swapping the pair moves the answer by more than thirty "
        "degrees, so a conversion that quietly reads them in the wrong order cannot pass here");

  Covers("one world space: a coordinate turned into a tile and back is the coordinate it "
         "started as, at every zoom and in all four quadrants");
  return Report();
}
