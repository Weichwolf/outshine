#include "Course.h"

#include <cmath>

namespace outshine::Pilot {

namespace {

constexpr double kTurn = 6.283185307179586;

double Wrapped(double angleRad) {
  while (angleRad > 0.5 * kTurn) { angleRad -= kTurn; }
  while (angleRad < -0.5 * kTurn) { angleRad += kTurn; }
  return angleRad;
}

double AwayFrom(const Placed &line, double eastM, double northM) {
  const double east = eastM - line.EastM;
  const double north = northM - line.NorthM;
  return east * east + north * north;
}

} // namespace

Placement Locate(const ReferenceLine &along, double eastM, double northM, double heightM,
                 double headingRad, double nearM, double windowM) {
  Placement out;
  out.EastM = eastM;
  out.NorthM = northM;
  out.HeightM = heightM;
  out.HeadingRad = headingRad;

  const double lengthM = along.LengthM();
  if (!(lengthM > 0.0) || !(windowM > 0.0)) { return out; }

  double lowM = nearM - windowM;
  double highM = nearM + windowM;
  if (lowM < 0.0) { lowM = 0.0; }
  if (highM > lengthM) { highM = lengthM; }
  if (!(highM > lowM)) { return out; }

  double bestM = lowM;
  double bestAway = 0.0;
  bool have = false;
  for (double atM = lowM; atM <= highM; atM += kResectionCoarseM) {
    Placed there;
    if (!along.At(atM, there)) { continue; }
    const double away = AwayFrom(there, eastM, northM);
    if (!have || away < bestAway) {
      have = true;
      bestAway = away;
      bestM = atM;
    }
  }
  if (!have) { return out; }

  double spanM = kResectionCoarseM;
  for (int narrow = 0; narrow < kResectionRefinements; ++narrow) {
    spanM *= 0.1;
    for (int side = -1; side <= 1; side += 2) {
      const double tryM = bestM + (double)side * spanM;
      if (tryM < lowM || tryM > highM) { continue; }
      Placed there;
      if (!along.At(tryM, there)) { continue; }
      const double away = AwayFrom(there, eastM, northM);
      if (away < bestAway) {
        bestAway = away;
        bestM = tryM;
      }
    }
  }

  Placed on;
  if (!along.At(bestM, on)) { return out; }
  const double left[2] = {-std::sin(on.HeadingRad), std::cos(on.HeadingRad)};
  out.Found = true;
  out.AlongM = bestM;
  out.OffsetM = (eastM - on.EastM) * left[0] + (northM - on.NorthM) * left[1];
  out.HeightErrorM = heightM - on.HeightM;
  out.HeadingErrorRad = Wrapped(headingRad - on.HeadingRad);
  out.CurvaturePerM = on.CurvaturePerM;
  out.SlopeAt = on.Slope;
  out.BankRad = on.BankRad;
  return out;
}

Sighting Sight(const ReferenceLine &along, const Placement &from, double chordM) {
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
    reachedM = std::sqrt(AwayFrom(there, from.EastM, from.NorthM));
    if (out.AtEnd || std::fabs(reachedM - chordM) < 1.0e-6) { break; }
    double stepM = chordM - reachedM;
    atM += stepM;
    if (atM > lengthM) {
      atM = lengthM;
      out.AtEnd = true;
    }
    if (atM < from.AlongM) { atM = from.AlongM; }
  }
  if (!along.At(atM, there)) { return out; }
  reachedM = std::sqrt(AwayFrom(there, from.EastM, from.NorthM));
  if (!(reachedM > 0.0)) { return out; }

  out.Found = true;
  out.OutOfReach = !out.AtEnd && std::fabs(reachedM - chordM) > 1.0e-3;
  out.AlongM = atM;
  out.ChordM = reachedM;
  out.BearingRad = std::atan2(there.NorthM - from.NorthM, there.EastM - from.EastM);
  out.ClimbRad = std::atan2(there.HeightM - from.HeightM, reachedM);
  return out;
}

} // namespace outshine::Pilot
