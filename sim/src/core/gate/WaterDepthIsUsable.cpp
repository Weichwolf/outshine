/* THE POSITIVE HALF, beside the negative one: without it the refusal below would pass for the wrong
 * reason the day the compile line breaks. */
#include "WaterDepth.h"

namespace outshine {

double DepthOrDry(const WaterDepth &d) {
  double m = 0.0;
  return d.TryDepthM(&m) ? m : 0.0;
}

} // namespace outshine
