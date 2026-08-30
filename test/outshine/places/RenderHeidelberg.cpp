#include "RenderedPlace.h"

// BlackForest -- A PLACE, RENDERED. This is not a proof and it scores nothing.
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
// is sub-pixel by construction. One height above sea level cannot show both a canyon and a street.
// Only the place and the bearing change, so a difference between two pictures is a difference
// between two places or a defect -- never the clock. The sun is declared at 60 deg of elevation
// bearing 180 deg rather than taken from the hour, because a real-time sun makes two pictures
// incomparable the moment they are rendered a few minutes apart.
//
// WHAT I EXPECT TO SEE, written before looking.
//   HEIDELBERG AND ITS CASTLE IN ONE FRAME, from the PHILOSOPHENWEG on the north bank of the Neckar
//   -- which is where the view is taken from and why it is the view. Eye at 49.4147 N 8.6968 E,
//   Schloss Heidelberg at 49.4106 N 8.7156 E, so the bearing is 108.50 deg and the distance 1 436
//   m, both derived from the two coordinates rather than chosen.
//
//   The old town lies BETWEEN them, on the far bank, so the frame should read in three depths: the
//   Neckar and the Alte Bruecke in the first half kilometre, the dense red roofs of the Altstadt
//   behind it, and the castle's red sandstone ruin on the Koenigstuhl's flank above them. The
//   castle is about 80 m higher than the town and 1.4 km out, so it sits a little above the middle
//   of the frame rather than on the horizon.
//
//   If the Altstadt is a green slope the OSM footprints did not reach the ring; if the castle is
//   missing while the town is not, it is the provider's `buildings` layer and not this engine.

int main(void) {
  return outshine::Test::RenderPlace(outshine::Test::Place{"Heidelberg", 49.4147, 8.6968, 108.50});
}
