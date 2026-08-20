#include "GroundSample.h"
// ACCEPTED

namespace outshine {

double GroundOrEye(const GroundSample &g, double eyeAslM) {
  double aslM = 0.0;
  return g.TryAslM(&aslM) ? aslM : eyeAslM;
}

}
