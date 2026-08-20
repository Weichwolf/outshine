#include "WaterDepth.h"
// REFUSED: calling a private constructor of class 'outshine::WaterDepth'

namespace outshine {

double Forbidden(double levelAslM, double groundAslM) {
  return WaterDepth(WaterDepth::State::Standing, levelAslM - groundAslM).M_;
}

}
