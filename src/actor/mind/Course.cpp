#include "Course.h"
#include "Angle.h"

#include <cmath>

namespace outshine::Pilot {

namespace {

Placed Beside(const Placed &on, double asideM) {
  if (asideM == 0.0) { return on; }
  Placed out = on;
  out.EastM -= std::sin(on.HeadingRad) * asideM;
  out.NorthM += std::cos(on.HeadingRad) * asideM;
  out.HeightM -= asideM * std::tan(on.BankRad);
  return out;
}

double AwayFrom(const Placed &line, double eastM, double northM) {
  const double east = eastM - line.EastM;
  const double north = northM - line.NorthM;
  return east * east + north * north;
}

}

Placement Locate(const ReferenceLine &along, double eastM, double northM, double heightM,
                 double headingRad, double nearM, double windowM) {
  Placement out;
  out.EastM = eastM;
  out.NorthM = northM;
  out.HeightM = heightM;
  out.HeadingRad = headingRad;

  double bestM = 0.0;
  if (!along.Nearest(eastM, northM, nearM, windowM, bestM)) { return out; }

  Placed on;
  if (!along.At(bestM, on)) { return out; }
  const double left[2] = {-std::sin(on.HeadingRad), std::cos(on.HeadingRad)};
  out.Found = true;
  out.AlongM = bestM;
  out.OffsetM = (eastM - on.EastM) * left[0] + (northM - on.NorthM) * left[1];
  out.HeightErrorM = heightM - on.HeightM;
  out.HeadingErrorRad = Wrapped(headingRad - on.HeadingRad);
  out.CurvaturePerM = on.CurvaturePerM;
  out.CurvatureRatePerM = on.CurvatureRatePerM;
  out.SlopeAt = on.Slope;
  out.BankRad = on.BankRad;
  return out;
}

Sighting Sight(const ReferenceLine &along, const Placement &from, double chordM,
               double asideM) {
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
    reachedM = std::sqrt(AwayFrom(Beside(there, asideM), from.EastM, from.NorthM));
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
  const Placed aimed = Beside(there, asideM);
  reachedM = std::sqrt(AwayFrom(aimed, from.EastM, from.NorthM));
  if (!(reachedM > 0.0)) { return out; }

  out.Found = true;
  out.OutOfReach = !out.AtEnd && std::fabs(reachedM - chordM) > 1.0e-3;
  out.AlongM = atM;
  out.ChordM = reachedM;
  out.BearingRad = std::atan2(aimed.NorthM - from.NorthM, aimed.EastM - from.EastM);
  out.ClimbRad = std::atan2(aimed.HeightM - from.HeightM, reachedM);
  return out;
}

}
