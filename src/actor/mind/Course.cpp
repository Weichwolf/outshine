#include <optional>
#include "math/Units.h"
#include "math/Vec2.h"
#include "Course.h"
#include "Angle.h"

#include <algorithm>
#include <cmath>

namespace outshine::Pilot {

constexpr double kOutOfReachM = 1.0e-3;

namespace {

Placed Beside(const Placed &on, double asideM) {
  if (asideM == 0.0) { return on; }
  Placed out = on;
  out.EastM -= std::sin(on.HeadingRad) * asideM;
  out.NorthM += std::cos(on.HeadingRad) * asideM;
  out.HeightM -= asideM * std::tan(on.BankRad);
  return out;
}

double AwayFrom(const Placed &line, EastNorth at) {
  const double east = at.EastM - line.EastM;
  const double north = at.NorthM - line.NorthM;
  return east * east + north * north;
}

} // namespace

Where Locate(const ReferenceLine &along, const Where &from, Nearby about) {
  const double eastM = from.EastM;
  const double northM = from.NorthM;
  Where out;
  out.EastM = eastM;
  out.NorthM = northM;
  out.HeightM = from.HeightM;
  out.HeadingRad = from.HeadingRad;

  const std::optional<double> found = along.Nearest({.EastM = eastM, .NorthM = northM}, about);
  if (!found) { return out; }
  const double bestM = *found;

  Placed on;
  if (!along.At(bestM, on)) { return out; }
  const Vec2 left = {{-std::sin(on.HeadingRad), std::cos(on.HeadingRad)}};
  out.Found = true;
  out.AlongM = bestM;
  out.OffsetM = (eastM - on.EastM) * left[0] + (northM - on.NorthM) * left[1];
  out.HeightErrorM = from.HeightM - on.HeightM;
  out.HeadingErrorRad = Wrapped(from.HeadingRad - on.HeadingRad);
  out.CurvaturePerM = on.CurvaturePerM;
  out.CurvatureRatePerM = on.CurvatureRatePerM;
  out.SlopeAt = on.Slope;
  out.BankRad = on.BankRad;
  return out;
}

Sighting Sight(const ReferenceLine &along, const Where &from, double chordM, double asideM) {
  Sighting out;
  if (!from.Found || !(chordM > 0.0)) { return out; }

  const double lengthM = along.LengthM();
  double atM = from.AlongM + chordM;
  if (atM > lengthM) {
    atM = lengthM;
    out.AtEnd = true;
  }

  Placed there;
  double reachedM = 0.0;
  for (int narrow = 0; narrow < kChordSteps; ++narrow) {
    if (!along.At(atM, there)) { return out; }
    reachedM =
        std::sqrt(AwayFrom(Beside(there, asideM), {.EastM = from.EastM, .NorthM = from.NorthM}));
    if (out.AtEnd || std::fabs(reachedM - chordM) < kLeastRunM) { break; }
    const double stepM = chordM - reachedM;
    atM += stepM;
    if (atM > lengthM) {
      atM = lengthM;
      out.AtEnd = true;
    }
    atM = std::max(atM, from.AlongM);
  }
  if (!along.At(atM, there)) { return out; }
  const Placed aimed = Beside(there, asideM);
  reachedM = std::sqrt(AwayFrom(aimed, {.EastM = from.EastM, .NorthM = from.NorthM}));
  if (!(reachedM > 0.0)) { return out; }

  out.Found = true;
  out.OutOfReach = !out.AtEnd && std::fabs(reachedM - chordM) > kOutOfReachM;
  out.AlongM = atM;
  out.ChordM = reachedM;
  out.BearingRad = std::atan2(aimed.NorthM - from.NorthM, aimed.EastM - from.EastM);
  out.ClimbRad = std::atan2(aimed.HeightM - from.HeightM, reachedM);
  return out;
}

} // namespace outshine::Pilot
