#include "RenderedPlace.h"

// Jura -- A PLACE, RENDERED. This is not a proof and it scores nothing.
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
//   SOLOTHURN WITH THE ALPS BEHIND IT, and the two are on ONE bearing rather than two.
//
//   THE STANDPOINT IS DERIVED FROM THE TARGETS, not chosen and then aimed. The line from the
//   JUNGFRAU at 46.5367 N 7.9626 E to SOLOTHURN at 47.2079 N 7.5371 E runs at 336.53 deg; carried
//   5 km past the town it reaches 47.2492 N 7.5108 E, on the Jura's south flank. Looking back down
//   that line at 156.53 deg puts Solothurn 5.0 km ahead and the Jungfrau 86.3 km beyond it -- the
//   SAME bearing, so the range stands behind the town rather than beside it.
//
//   FOUR EARLIER FRAMINGS FAILED AND EACH FAILED DIFFERENTLY, which is why the standpoint is
//   computed now instead of picked. From the Hotel Weissenstein aimed at the Jungfrau the town
//   falls outside a 55 deg frame; aimed at the town the range does; due south the ridge's own crest
//   fills it; and a hundred metres of extra height clears the crest but not the next Jura fold. Two
//   targets 32 deg apart cannot both sit in a 55 deg frame from a standpoint that is not on their
//   line. Putting the eye ON the line makes the angle zero.
//
//   So the frame should read in four depths: the flank falling away in the first kilometres,
//   SOLOTHURN on the Aare at 5 km a little below centre, the MITTELLAND carrying the middle
//   distance for fifty, and the BERNESE ALPS pale along the horizon at eighty-six. This is the
//   set's test of DEPTH: nothing else in it asks a frame to hold four scales at once.
//
//   If the Alps are the same green as the plain, the air is costing nothing. If Solothurn is a
//   green patch, the OSM footprints did not reach the ring.

int main(void) {
  return outshine::Test::RenderPlace(outshine::Test::Place{"Jura", 47.2492, 7.5108, 156.53});
}
