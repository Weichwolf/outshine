#ifndef OUTSHINE_ACTOR_MIND_RAIL_H
#define OUTSHINE_ACTOR_MIND_RAIL_H

#include "Pilot.h"
#include "SpeedProfile.h"

namespace outshine::Pilot {

struct Rails {
  double CantDeficiencyMs2 = 0.0;
  double GaugeM = 0.0;
  double CentreOfMassM = 0.0;
};

struct Haul {
  double DriveN = 0.0;
  double BrakeN = 0.0;
  double UnbalancedMs2 = 0.0;
  bool PastCant = false;
  bool Overturns = false;
};

[[nodiscard]] double OverturningMs2(const Rails &on, double gravityMs2);
[[nodiscard]] Haul
Ride(const Rails &on, const Envelope &within, const Demand &asked, const Where &at, double speedMs);

}

#endif
