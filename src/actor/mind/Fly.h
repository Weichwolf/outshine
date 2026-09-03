#ifndef OUTSHINE_ACTOR_MIND_FLY_H
#define OUTSHINE_ACTOR_MIND_FLY_H

#include "Pilot.h"
#include "SpeedProfile.h"

namespace outshine::Pilot {

struct Wings {
  double BankLimitRad = 0.0;
  double LoadFactorLimit = 0.0;
  double ClimbLimitRad = 0.0;
};

struct Flying {
  double BankRad = 0.0;
  double ClimbRad = 0.0;
  double LoadFactor = 1.0;
  double ThrustN = 0.0;
};

[[nodiscard]] double BankLimitOf(const Wings &of);
[[nodiscard]] double TightestPerM(const Wings &of, double speedMs, double gravityMs2);
[[nodiscard]] Flying Fly(const Wings &of, const Envelope &within, const Demand &asked);

} // namespace outshine::Pilot

#endif
