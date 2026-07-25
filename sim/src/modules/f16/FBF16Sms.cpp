#include "FBF16Sms.h"

namespace FlightBox {

void FBF16Sms::Run(FBState &state, double dt) {
  (void)dt;
  state.armState = State;
}

} // namespace FlightBox
