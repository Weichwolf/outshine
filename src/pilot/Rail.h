#ifndef OUTSHINE_PILOT_RAIL_H
#define OUTSHINE_PILOT_RAIL_H

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

[[nodiscard]] double OverturningMs2(const Rails &on);
[[nodiscard]] Haul Ride(const Rails &on, const Envelope &within, const Demand &asked,
                        const Placement &at, double speedMs);

} // namespace outshine::Pilot

#endif
