/* THE POSITIVE HALF. Without it the negative halves beside it would pass for the wrong reason the day
 * the compile line breaks. */
#include "GroundSample.h"

namespace outshine {

double GroundOrEye(const GroundSample &g, double eyeAslM) {
  double aslM = 0.0;
  return g.TryAslM(&aslM) ? aslM : eyeAslM;
}

} // namespace outshine
