#ifndef OUTSHINE_ACTOR_MIND_WALK_H
#define OUTSHINE_ACTOR_MIND_WALK_H

#include "Pilot.h"
#include "SpeedProfile.h"

namespace outshine::Pilot {

struct Gait {
  double TurnRateRadS = 0.0;
  double TopMs = 0.0;
  double AccelMs2 = 0.0;
};

struct Stride {
  double HeadingRateRadS = 0.0;
  double SpeedMs = 0.0;
  double AlongMs2 = 0.0;
};

[[nodiscard]] double TightestPerM(const Gait &of, double speedMs);
[[nodiscard]] Stride Walk(const Gait &of, const Demand &asked, double speedMs);

}

#endif
