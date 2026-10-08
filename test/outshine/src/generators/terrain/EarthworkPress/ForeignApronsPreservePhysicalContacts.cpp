#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

outshine::EarthworkStamp Contact(double lowN, double bedM, bool profile, bool pad) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {-50, lowN, 50, lowN, 50, lowN + 4, -50, lowN + 4};
  stamp.LowE = -50;
  stamp.HighE = 50;
  stamp.LowN = lowN;
  stamp.HighN = lowN + 4;
  stamp.PlateauM = bedM;
  stamp.ApronM = 20;
  stamp.YieldM = std::abs(bedM);
  stamp.CorridorKey = lowN < 0 ? 1u : 2u;
  stamp.Fills = true;
  stamp.Kind = pad ? outshine::EarthworkKind::Pad : outshine::EarthworkKind::Corridor;
  if (profile) {
    stamp.Profile = outshine::ProfiledCorridorSpan{.CorridorKey = stamp.CorridorKey,
                                                   .BeginM = {.EastM = -50, .NorthM = lowN + 2},
                                                   .EndM = {.EastM = 50, .NorthM = lowN + 2},
                                                   .BeginDerivativeM = {.EastM = 100},
                                                   .EndDerivativeM = {.EastM = 100},
                                                   .StationLengthM = 100,
                                                   .BeginBedM = bedM,
                                                   .EndBedM = bedM,
                                                   .BeginPavementHalfWidthM = 2,
                                                   .EndPavementHalfWidthM = 2,
                                                   .BeginHalfWidthM = 2,
                                                   .EndHalfWidthM = 2};
  }
  return stamp;
}

void CheckShoulders() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double stepM = 0.001;
  const std::array<EastNorth, 3> points{
      {{.NorthM = 2 - stepM}, {.NorthM = 2}, {.NorthM = 2 + stepM}}};
  for (const bool profile : {false, true}) {
    for (const bool pad : {false, true}) {
      for (const bool reverse : {false, true}) {
        for (const double sourceM : {-10., 0., 10.}) {
          for (const double bedM : {-6., 6.}) {
            std::array stamps{Contact(-2, bedM, profile, false), Contact(10, -bedM, false, pad)};
            if (reverse) { std::ranges::reverse(stamps); }
            std::array heights{sourceM, sourceM, sourceM};
            const auto result = ApplyEarthworkStamps(stamps, points, heights, kMostEarthworkM);
            CHECK(result.Structures == 0, "all proposed contacts fit the height bound");
            CHECK(std::abs(heights[0] - bedM) < 1.e-6 && std::abs(heights[1] - bedM) < 1.e-6,
                  "foreign road and foundation aprons cannot replace physical road support");
            CHECK(std::abs(heights[2] - heights[1]) / stepM < 0.001,
                  "mixed apron kinds meet the shoulder with continuous height and slope");
            std::array staged{sourceM, sourceM, sourceM};
            EarthworkPressJob job(stamps, points, staged, kMostEarthworkM);
            while (!job.Advance(1)) {}
            (void)job.Take();
            CHECK(staged == heights, "bounded work resolves the same contact and apron field");
          }
        }
      }
    }
  }
}

void CheckConstraints() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<EastNorth, 1> point{{{.NorthM = 0}}};
  for (const bool reverse : {false, true}) {
    std::array cores{Contact(-2, 10, false, false), Contact(-2, 8, false, true)};
    if (reverse) { std::ranges::reverse(cores); }
    std::array<double, 1> heights{0};
    (void)ApplyEarthworkStamps(cores, point, heights, kMostEarthworkM);
    CHECK(heights[0] == 8, "real overlapping support cores retain their terrain constraints");
    std::array basin{Contact(-2, -2, false, true), Contact(10, 6, false, false)};
    basin[0].Kind = EarthworkKind::Basin;
    basin[0].ApronM = 0;
    if (reverse) { std::ranges::reverse(basin); }
    for (const double sourceM : {-4., 0.}) {
      heights[0] = sourceM;
      (void)ApplyEarthworkStamps(basin, point, heights, kMostEarthworkM);
      CHECK(heights[0] == std::min(sourceM, -2.), "outside fills cannot raise a basin floor");
    }
  }
  auto cut = Contact(10, 6, false, true);
  cut.Fills = false;
  std::array<double, 1> heights{0};
  (void)ApplyEarthworkStamps(std::span(&cut, 1), point, heights, kMostEarthworkM);
  CHECK(heights[0] == 0, "cut-only aprons never add soil");
}

}

int main() {
  CheckShoulders();
  CheckConstraints();
  return outshine::Test::Report();
}
