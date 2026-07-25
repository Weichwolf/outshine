#include "FBF16Ufc.h"

namespace FlightBox {

void FBF16Ufc::Run(FBState &state, double dt) {
  (void)dt;
  state.alowFt = AlowFt;
  state.steerNum = StNum;
}

} // namespace FlightBox
