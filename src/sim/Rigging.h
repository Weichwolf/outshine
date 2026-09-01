#ifndef OUTSHINE_SIM_RIGGING_H
#define OUTSHINE_SIM_RIGGING_H

#include <string>

#include <Scenario.h>

#include "math/Vec3.h"
#include "Drive.h"
#include "Rig.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

struct Rigged {
  bool Stood = false;
  Physics::Rig Rig;
  Pilot::Axles Axles;
  Envelope Envelope;
  double TightestM = 0.0;
  Vec3 CentreM;
  Vec3 SeatM;
  double StandsAtM = 0.0;
  double MetresPerAssetUnit = 0.0;
  Vec3 ModelShiftM;
  std::string Error;
};

[[nodiscard]] Rigged Stand(const Body &declared, double gravityMs2, double airDensityKgM3);

} // namespace outshine::Sim

#endif
