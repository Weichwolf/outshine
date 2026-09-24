#include "Check.h"
#include "EarthworkPress.h"
#include "GroundMesher.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace {

constexpr double kRadiusM = 12.0;
constexpr double kArcLengthM = kRadiusM * std::numbers::pi / 2.0;
constexpr double kBedBeginM = 100.0;
constexpr double kGrade = 0.05;

outshine::EarthworkStamp Span(double fromM, double toM) {
  const double fromAngle = fromM / kRadiusM;
  const double toAngle = toM / kRadiusM;
  const double lengthM = toM - fromM;
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {-6.0, -6.0, 18.0, -6.0, 18.0, 18.0, -6.0, 18.0};
  stamp.LowE = -6.0;
  stamp.HighE = 18.0;
  stamp.LowN = -6.0;
  stamp.HighN = 18.0;
  stamp.ApronM = 6.0;
  stamp.Fills = true;
  stamp.Kind = outshine::EarthworkKind::Corridor;
  stamp.Profile = outshine::ProfiledCorridorSpan{
      .CorridorKey = 73,
      .BeginM = {.EastM = kRadiusM * std::cos(fromAngle), .NorthM = kRadiusM * std::sin(fromAngle)},
      .EndM = {.EastM = kRadiusM * std::cos(toAngle), .NorthM = kRadiusM * std::sin(toAngle)},
      .BeginDerivativeM = {.EastM = -lengthM * std::sin(fromAngle),
                           .NorthM = lengthM * std::cos(fromAngle),
                           .UpM = kGrade * lengthM},
      .EndDerivativeM = {.EastM = -lengthM * std::sin(toAngle),
                         .NorthM = lengthM * std::cos(toAngle),
                         .UpM = kGrade * lengthM},
      .StationLengthM = lengthM,
      .BeginBedM = kBedBeginM + kGrade * fromM,
      .EndBedM = kBedBeginM + kGrade * toM,
      .BeginPavementHalfWidthM = 3.5,
      .EndPavementHalfWidthM = 3.5,
      .BeginHalfWidthM = 4.5,
      .EndHalfWidthM = 4.5};
  return stamp;
}

std::vector<outshine::EarthworkStamp> Partition(double stepM) {
  std::vector<outshine::EarthworkStamp> stamps;
  for (double fromM = 0.0; fromM < kArcLengthM; fromM += stepM) {
    stamps.push_back(Span(fromM, std::min(fromM + stepM, kArcLengthM)));
  }
  return stamps;
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
  const auto coarse = Partition(2.0);
  const auto fine = Partition(1.0);
  for (const double radialM : {2.5, kRadiusM, 21.5}) {
    std::vector<EastNorth> points;
    for (int station = 5; station <= 152; ++station) {
      const double angle = static_cast<double>(station) * 0.01;
      points.push_back({.EastM = radialM * std::cos(angle), .NorthM = radialM * std::sin(angle)});
    }
    const auto coarseHeights = Press(coarse, points);
    const auto fineHeights = Press(fine, points);
    double partitionErrorM = 0.0;
    double greatestImpulseM = 0.0;
    double contactErrorM = 0.0;
    for (size_t at = 0; at < points.size(); ++at) {
      partitionErrorM = std::max(partitionErrorM, std::abs(coarseHeights[at] - fineHeights[at]));
      if (radialM == kRadiusM) {
        const double stationM = (static_cast<double>(at) + 5.0) * 0.01 * kRadiusM;
        contactErrorM =
            std::max(contactErrorM, std::abs(fineHeights[at] - (kBedBeginM + kGrade * stationM)));
      }
      if (at > 0 && at + 1 < points.size()) {
        greatestImpulseM =
            std::max(greatestImpulseM,
                     std::abs(fineHeights[at + 1] - 2.0 * fineHeights[at] + fineHeights[at - 1]));
      }
    }
    Note("curved shoulder partition difference", partitionErrorM, "m");
    Note("curved shoulder greatest impulse", greatestImpulseM, "m/0.12m arc");
    CHECK(partitionErrorM < 0.0001 && greatestImpulseM < 0.0001,
          "both sides of a tight graded bend keep one smooth terrain profile");
    if (radialM == kRadiusM) {
      Note("curved centreline contact error", contactErrorM, "m");
      CHECK(contactErrorM < 0.0001,
            "the curved centreline meets its analytic grade at every station");
    }
  }
  return Report();
}
