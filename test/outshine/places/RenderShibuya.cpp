#include "RenderedPlace.h"

// Shibuya -- A PLACE, RENDERED. This is not a proof and it scores nothing.
//
// `places/` exists so the engine's visual state is VISIBLE: every gate drops six pictures into
// build/places/ and an eye decides. The only thing a case here refuses on is not getting a picture
// out at all -- a declaration that will not stand, a world that will not compose, a frame that
// cannot be written. Whether the picture is any GOOD is the owner's judgement and never a number
// invented here.
//
// THE SIX ARE COMPARABLE BY CONSTRUCTION. Same eye height above the GROUND, same sun, same lens,
// same frame. Above the ground rather than above the sea, and the reason is arithmetic: at 720 px
// over 55 deg a pixel is 0.076 deg, so a 12 m building needs to be within about 3 km to cover three
// of them -- and from 4 000 m ASL at -15 deg of pitch the nearest ground is 14.9 km away, so a town
// is sub-pixel by construction. One height above sea level cannot show both a canyon and a street. Only the place and the bearing change, so a difference between two pictures is a
// difference between two places or a defect -- never the clock. The sun is declared at 60 deg of
// elevation bearing 180 deg rather than taken from the hour, because a real-time sun makes two
// pictures incomparable the moment they are rendered a few minutes apart.
//
// WHAT I EXPECT TO SEE, written before looking.
//   The densest footprint count of the six, 91 208. At 240 km of sight Fuji stands 100 km away and
//   3 776 m up, so it must be a silhouette on the horizon.

int main(void) {
  return outshine::Test::RenderPlace(
      outshine::Test::Place{"Shibuya", 35.6595, 139.7005, 40.0});
}
