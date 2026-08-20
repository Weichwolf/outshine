#include "GroundSample.h"
// REFUSED: ignoring return value of function declared with 'nodiscard' attribute

namespace outshine {

double Forbidden(const GroundSample &g) {
  double aslM = 0.0;
  g.TryAslM(&aslM);
  return aslM;
}

}
