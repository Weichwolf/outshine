/* THE GATE, and it must FAIL to build. A depth read without its state is how a gorge answered
 * -4.371 m; there is no member to read and no constructor that takes a signed metre. */
#include "WaterDepth.h"

namespace outshine {

double Forbidden(double levelAslM, double groundAslM) {
  return WaterDepth(WaterDepth::State::Standing, levelAslM - groundAslM).M_;
}

} // namespace outshine
