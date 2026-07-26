#include "FBF16Ufc.h"

namespace FlightBox {

void FBF16Ufc::Run(FBState &state, double dt) {
  (void)dt;
  state.Ufc.AlowFt = AlowFt;
  state.Ufc.BingoLbs = BingoLbs;
  state.Ufc.BingoEffectiveLbs = EffectiveBingo();
  state.Ufc.SteerNum = StNum;
  state.Ufc.H.Publish(state.NowS);
}

} // namespace FlightBox
