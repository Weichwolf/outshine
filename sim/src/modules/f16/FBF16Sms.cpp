#include "FBF16Sms.h"

namespace FlightBox {

void FBF16Sms::Run(FBState &state, double dt) {
  (void)dt;
  state.Stores.Arm = State;
  state.Stores.H.Publish(state.NowS);
}

} // namespace FlightBox
