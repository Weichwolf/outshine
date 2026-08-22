#ifndef OUTSHINE_ACTOR_MIND_COURSE_H
#define OUTSHINE_ACTOR_MIND_COURSE_H

#include "ReferenceLine.h"

namespace outshine::Pilot {

inline constexpr int kChordSteps = 12;

struct Placement {
  bool Found = false;
  double EastM = 0.0;
  double NorthM = 0.0;
  double HeightM = 0.0;
  double HeadingRad = 0.0;
  double AlongM = 0.0;
  double OffsetM = 0.0;
  double HeightErrorM = 0.0;
  double HeadingErrorRad = 0.0;
  double CurvaturePerM = 0.0;
  double CurvatureRatePerM = 0.0;
  double SlopeAt = 0.0;
  double BankRad = 0.0;
};

[[nodiscard]] Placement Locate(const ReferenceLine &along, double eastM, double northM,
                               double heightM, double headingRad, double nearM, double windowM);

struct Sighting {
  bool Found = false;
  bool AtEnd = false;
  bool OutOfReach = false;
  double AlongM = 0.0;
  double ChordM = 0.0;
  double BearingRad = 0.0;
  double ClimbRad = 0.0;
};

[[nodiscard]] Sighting Sight(const ReferenceLine &along, const Placement &from, double chordM,
                             double asideM = 0.0);

} // namespace outshine::Pilot

#endif
