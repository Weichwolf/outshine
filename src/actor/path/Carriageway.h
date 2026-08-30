#ifndef OUTSHINE_ACTOR_PATH_CARRIAGEWAY_H
#define OUTSHINE_ACTOR_PATH_CARRIAGEWAY_H

#include "ReferenceLine.h"

namespace outshine {

struct Astride {
  bool On = false;
  double AlongM = 0.0;
  double AcrossM = 0.0;
  double HeightM = 0.0;
  double NormalM[3] = {0.0, 1.0, 0.0};
};

[[nodiscard]] Astride Stand(const ReferenceLine &over,
                            double eastM,
                            double northM,
                            double halfWidthM,
                            double nearM,
                            double windowM);

[[nodiscard]] Astride
StandAt(const ReferenceLine &over, double alongM, double acrossM, double halfWidthM);

} // namespace outshine

#endif
