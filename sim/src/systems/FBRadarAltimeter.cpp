#include "FBRadarAltimeter.h"

namespace FlightBox {

namespace {
constexpr float kMToFt = 3.280839895f;
} // namespace

void FBRadarAltimeter::Run(FBState &state, float elevAslM, float groundAslM) {
  /* Unpowered: the block goes Invalid and KEEPS its last number — a consumer that ignores the head
   * must not silently get a fresh-looking zero (see the class banner). */
  if (!Powered_) { state.RadarAlt.H.Invalidate(); return; }
  state.RadarAlt.AglFt = (elevAslM - groundAslM) * kMToFt;
  state.RadarAlt.H.Publish(state.NowS);
}

} // namespace FlightBox
