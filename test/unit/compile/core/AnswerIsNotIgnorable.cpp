/* THE GATE, and it must FAIL to build. TryAslM's answer discarded leaves the caller reading whatever
 * its own variable held — the sentinel comparison again, spelled differently. -Werror is what makes
 * this one a failure rather than a warning, and -Werror is the house build. */
#include "GroundSample.h"
// REFUSED: ignoring return value of function declared with 'nodiscard' attribute

namespace outshine {

double Forbidden(const GroundSample &g) {
  double aslM = 0.0;
  g.TryAslM(&aslM);
  return aslM;
}

} // namespace outshine
