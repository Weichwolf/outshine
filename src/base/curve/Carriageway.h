#ifndef OUTSHINE_BASE_CURVE_CARRIAGEWAY_H
#define OUTSHINE_BASE_CURVE_CARRIAGEWAY_H

#include "math/Vec3.h"
#include "ReferenceLine.h"

namespace outshine {

struct Astride {
  bool On = false;
  double AlongM = 0.0;
  double AcrossM = 0.0;
  double HeightM = 0.0;
  Vec3 NormalM = {{0.0, 1.0, 0.0}};
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
