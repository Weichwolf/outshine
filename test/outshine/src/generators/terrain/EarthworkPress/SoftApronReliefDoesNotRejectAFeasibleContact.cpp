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
  for (const auto kind : {EarthworkKind::Pad, EarthworkKind::Corridor, EarthworkKind::Clearance}) {
    for (const double direction : {-1., 1.}) {
      EarthworkStamp contact;
      contact.RingEastNorthM = {-1, -1, 1, -1, 1, 1, -1, 1};
      contact.LowE = contact.LowN = -1;
      contact.HighE = contact.HighN = 1;
      contact.ApronM = 3;
      contact.Fills = true;
      contact.Kind = kind;
      const std::array source{10 * direction, 100 * direction, 100 * direction};
      auto heights = source;
      const auto pressed = ApplyEarthworkStamps(std::span{&contact, 1u}, points, heights, 30);
      CHECK(pressed.Structures == 0 && pressed.Refused.front() == 0 && pressed.Held == 0,
            "source relief in a soft apron cannot reject a feasible physical contact");
      const bool cuts = kind != EarthworkKind::Clearance || direction > 0;
      CHECK(heights[0] == (cuts ? 0 : source[0]) && heights[2] == source[2],
            "the physical core is honored while distant source terrain remains unchanged");
      CHECK(std::abs(heights[1] - source[1]) <= 30 &&
                (cuts ? std::abs(heights[1]) < std::abs(source[1]) : heights[1] == source[1]),
            "soft correction stays inside the per-node limit and clearance never fills");
      auto sliced = source;
      EarthworkPressJob job({&contact, 1u}, points, sliced, 30);
      bool done = false;
      for (int step = 0; step < 100 && !done; ++step) { done = job.Advance(1); }
      CHECK(done && sliced == heights, "sliced rejection and pressing use the same decision");
      (void)job.Take();
      heights = {31 * direction, source[1], source[2]};
      const auto excessive = heights;
      const auto refused = ApplyEarthworkStamps(std::span{&contact, 1u}, points, heights, 30);
      CHECK(cuts ? refused.Structures == 1 && heights == excessive
                 : refused.Structures == 0 && heights == excessive,
            "excessive physical correction still rejects the whole contact before mutation");
    }
  }
  return Report();
}
