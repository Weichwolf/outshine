#include "FBRadarAltimeter.h"

namespace FlightBox {

namespace {
constexpr float kMToFt = 3.280839895f;
} // namespace

void FBRadarAltimeter::Run(FBState &state, float elevAslM, float groundAslM) {
  state.radarAltFt = (elevAslM - groundAslM) * kMToFt;
}

} // namespace FlightBox
