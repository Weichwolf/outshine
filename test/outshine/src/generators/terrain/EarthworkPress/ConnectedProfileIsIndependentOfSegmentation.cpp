#include "Check.h"
#include "EarthworkPress.h"
#include "GroundMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace {

outshine::EarthworkStamp Span(double fromM, double toM, uint64_t corridorKey = 42) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {fromM, -2.0, toM, -2.0, toM, 2.0, fromM, 2.0};
  stamp.LowE = fromM;
  stamp.HighE = toM;
  stamp.LowN = -2.0;
  stamp.HighN = 2.0;
  stamp.ApronM = 2.0;
  stamp.Fills = true;
  stamp.Kind = outshine::EarthworkKind::Corridor;
  stamp.Profile = outshine::ProfiledCorridorSpan{
      .CorridorKey = corridorKey,
      .BeginM = {.EastM = fromM, .NorthM = 0.0},
      .EndM = {.EastM = toM, .NorthM = 0.0},
      .BeginDerivativeM = {.EastM = toM - fromM, .NorthM = 0.0, .UpM = 0.1 * (toM - fromM)},
      .EndDerivativeM = {.EastM = toM - fromM, .NorthM = 0.0, .UpM = 0.1 * (toM - fromM)},
      .StationLengthM = toM - fromM,
      .BeginBedM = 100.0 + 0.1 * fromM,
      .EndBedM = 100.0 + 0.1 * toM,
      .BeginPavementHalfWidthM = 1.0,
      .EndPavementHalfWidthM = 1.0,
      .BeginHalfWidthM = 2.0,
      .EndHalfWidthM = 2.0};
  return stamp;
}

std::vector<double> Press(std::span<const outshine::EarthworkStamp> stamps,
                          std::span<const outshine::EastNorth> points) {
  std::vector<double> heights(points.size(), 99.0);
  (void)outshine::ApplyEarthworkStamps(stamps, points, heights, outshine::kMostEarthworkM);
  return heights;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<EastNorth> points;
  for (int step = 0; step <= 12; ++step) {
    points.push_back({.EastM = 0.5 + 0.25 * static_cast<double>(step), .NorthM = 1.5});
  }
  const std::array coarse{Span(0.0, 2.0), Span(2.0, 4.0)};
  const std::array fine{Span(0.0, 1.0), Span(1.0, 2.0), Span(2.0, 3.0), Span(3.0, 4.0)};
  const auto coarseHeights = Press(coarse, points);
  const auto fineHeights = Press(fine, points);
  std::array reversed = fine;
  std::ranges::reverse(reversed);
  const auto reversedHeights = Press(reversed, points);
  double partitionErrorM = 0.0;
  double orderErrorM = 0.0;
  double greatestImpulseM = 0.0;
  for (size_t at = 0; at < points.size(); ++at) {
    partitionErrorM = std::max(partitionErrorM, std::abs(coarseHeights[at] - fineHeights[at]));
    orderErrorM = std::max(orderErrorM, std::abs(fineHeights[at] - reversedHeights[at]));
    if (at > 0 && at + 1 < points.size()) {
      greatestImpulseM =
          std::max(greatestImpulseM,
                   std::abs(fineHeights[at + 1] - 2.0 * fineHeights[at] + fineHeights[at - 1]));
    }
  }
  Note("profile partition difference", partitionErrorM, "m");
  Note("profile order difference", orderErrorM, "m");
  Note("profile greatest impulse", greatestImpulseM, "m/0.25m");
  CHECK(partitionErrorM < 0.002 && orderErrorM < 1e-9 && greatestImpulseM < 0.005,
        "connected graded corridor has one smooth field independent of subdivision and order");

  auto unrelated = Span(1.0, 3.0, 99);
  unrelated.Profile->BeginBedM += 10.0;
  unrelated.Profile->EndBedM += 10.0;
  std::array withUnrelated{coarse[0], coarse[1], unrelated};
  const auto isolated = Press(withUnrelated, points);
  CHECK(std::abs(isolated.front() - coarseHeights.front()) < 1e-9,
        "a farther disconnected corridor does not enter the nearest route's height average");
  return Report();
}
