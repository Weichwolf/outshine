#ifndef OUTSHINE_ACTOR_MIND_PILOT_H
#define OUTSHINE_ACTOR_MIND_PILOT_H

#include "Course.h"

namespace outshine::Pilot {

inline constexpr double kSettleS = 1.0;

struct Holding {
  double SettleS = 0.0;
  double LeastReachM = 0.0;
  double TightestPerM = 0.0;
  double HoldWithinM = 0.0;
  double AsideM = 0.0;
};

struct Demand {
  bool Held = false;
  bool AtEnd = false;
  bool OutOfReach = false;
  double CurvaturePerM = 0.0;
  double AskedPerM = 0.0;
  double SpeedMs = 0.0;
  double AlongMs2 = 0.0;
  double ClimbRad = 0.0;
  double ReachM = 0.0;
  bool Saturated = false;
};

struct Travelling {
  double SpeedMs = 0.0;
  double CurvatureRatePerM = 0.0;
};

[[nodiscard]] double ReachOf(const Holding &with, Travelling how);

[[nodiscard]] Demand Hold(const ReferenceLine &along,
                          const Holding &with,
                          const Where &at,
                          double speedMs,
                          double wantedMs);

} // namespace outshine::Pilot

#endif
