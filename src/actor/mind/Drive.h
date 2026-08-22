#ifndef OUTSHINE_ACTOR_MIND_DRIVE_H
#define OUTSHINE_ACTOR_MIND_DRIVE_H

#include "Pilot.h"
#include "SpeedProfile.h"

namespace outshine::Pilot {

struct Axles {
  double WheelbaseM = 0.0;
  double SteerLimitRad = 0.0;
};

struct Steering {
  double SteerRad = 0.0;
  double DriveN = 0.0;
  double BrakeN = 0.0;
};

[[nodiscard]] double TightestPerM(const Axles &of, const Envelope &within, double speedMs);
[[nodiscard]] Steering Drive(const Axles &of, const Envelope &within, const Demand &asked);

} // namespace outshine::Pilot

#endif
