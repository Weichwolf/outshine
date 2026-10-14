#include "Check.h"
#include "EarthworkPress.h"

#include <array>
#include <cmath>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<EastNorth, 3> points{
      {{.EastM = 0, .NorthM = 0}, {.EastM = 2, .NorthM = 0}, {.EastM = 100, .NorthM = 0}}};
  const std::array source{10.0, 100.0, 100.0};
  for (const auto kind : {EarthworkKind::Pad, EarthworkKind::Corridor, EarthworkKind::Clearance}) {
    EarthworkStamp contact;
    contact.RingEastNorthM = {-1, -1, 1, -1, 1, 1, -1, 1};
    contact.LowE = contact.LowN = -1;
    contact.HighE = contact.HighN = 1;
    contact.ApronM = 3;
    contact.PlanarSupportM = 1.5;
    contact.Fills = true;
    contact.Kind = kind;
    auto heights = source;
    const auto pressed = ApplyEarthworkStamps(std::span{&contact, 1u}, points, heights, 30);
    CHECK(pressed.Structures == 0 && pressed.Refused.front() == 0 && pressed.Held == 0 &&
              heights[0] == 0,
          "bounded auxiliary support preserves the feasible physical contact");
    CHECK(heights[1] < source[1] && source[1] - heights[1] <= 30,
          "auxiliary planar cuts respect the same per-node budget as soft aprons");
    CHECK(heights[2] == source[2], "terrain outside both apron and support stays unchanged");
    auto sliced = source;
    EarthworkPressJob job({&contact, 1u}, points, sliced, 30);
    bool done = false;
    for (int step = 0; step < 100 && !done; ++step) { done = job.Advance(1); }
    CHECK(done && sliced == heights, "sliced and synchronous support use the same budget");
    (void)job.Take();
  }
  return Report();
}
