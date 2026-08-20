#include "WaterDepth.h"
// ACCEPTED

namespace outshine {

double DepthOrDry(const WaterDepth &d) {
  double m = 0.0;
  return d.TryDepthM(&m) ? m : 0.0;
}

}
