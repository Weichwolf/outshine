#include "Check.h"
#include "GroundYield.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  EarthworkStamp accepted;
  accepted.RingEastNorthM = {-1.0, -1.0, 3.0, -1.0, 3.0, 1.0, -1.0, 1.0};
  accepted.LowE = -1.0;
  accepted.HighE = 3.0;
  accepted.LowN = -1.0;
  accepted.HighN = 1.0;
  accepted.PlateauM = 5.0;
  accepted.Fills = true;

  EarthworkStamp rejected = accepted;
  rejected.PlateauM = -5.0;
  rejected.SlopeE = -15.0;
  const std::array<EarthworkStamp, 2> stamps{accepted, rejected};
  const std::array<EastNorth, 3> points{{{.EastM = 0.0, .NorthM = 0.0},
                                         {.EastM = 1.0, .NorthM = 0.0},
                                         {.EastM = 2.0, .NorthM = 0.0}}};
  std::array<double, 3> heights{};
  const Pressed pressed = PressPoints(stamps, points, heights, 30.0);
  CHECK(pressed.Structures == 1 && pressed.Refused[1] == 1 && pressed.Moved == 3 &&
            pressed.Held == 0,
        "late excessive cut rejects its stamp globally before any height changes");
  CHECK(std::ranges::all_of(heights, [](double height) { return height == 5.0; }) &&
            std::ranges::all_of(pressed.DecidedBy, [](uint32_t which) { return which == 0; }) &&
            pressed.Inside.size() == points.size(),
        "earlier nodes and ordered claims belong only to the accepted stamp");

  for (const size_t slice : {1u, 2u, 7u}) {
    heights.fill(0.0);
    PressPointsJob job(stamps, points, heights, 30.0);
    bool complete = false;
    for (int step = 0; step < 16 && !complete; ++step) {
      complete = job.Advance(slice);
      if (step == 0) {
        CHECK(std::ranges::all_of(heights, [](double height) { return height == 0.0; }),
              "the rejection phase leaves all source heights unchanged");
      }
    }
    CHECK(complete && job.HeapBytes() > 0, "the bounded job completes and reports retained memory");
    const Pressed staged = job.Take();
    CHECK(staged.Moved == pressed.Moved && staged.Structures == pressed.Structures &&
              staged.Held == pressed.Held && staged.Refused == pressed.Refused &&
              staged.DecidedBy == pressed.DecidedBy &&
              staged.Inside.size() == pressed.Inside.size() &&
              std::ranges::equal(staged.Inside,
                                 pressed.Inside,
                                 [](const Covered &left, const Covered &right) {
                                   return left.Point == right.Point && left.Stamp == right.Stamp;
                                 }) &&
              std::ranges::all_of(heights, [](double height) { return height == 5.0; }),
          "different interruption sizes preserve decisions, claims and final heights");
  }

  heights.fill(0.0);
  const Pressed permitted = PressPoints(stamps, points, heights, 100.0);
  const std::array<double, 3> expectedHeights{-5.0, -20.0, -35.0};
  CHECK(permitted.Structures == 0 && permitted.Moved == 3 && heights == expectedHeights,
        "the late stamp changes all nodes when the earthwork bound permits it");
  return Report();
}
