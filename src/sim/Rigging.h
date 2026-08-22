#ifndef OUTSHINE_SIM_RIGGING_H
#define OUTSHINE_SIM_RIGGING_H

#include <string>

#include <outshine/Scenario.h>

#include "Drive.h"
#include "Rig.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

struct Rigged {
  bool Stood = false;
  Physics::Rig Rig;
  Pilot::Axles Axles;
  Envelope Envelope;
  double CentreM[3] = {0.0, 0.0, 0.0};
  double SeatM[3] = {0.0, 0.0, 0.0};
  std::string Error;
};

[[nodiscard]] Rigged Stand(const Vehicle &declared);

} // namespace outshine::Sim

#endif
