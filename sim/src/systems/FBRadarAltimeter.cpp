#include "FBRadarAltimeter.h"

namespace FlightBox::Systems {

namespace {
constexpr float kMToFt = 3.280839895f;
} // namespace

void FBRadarAltimeter::Run(FBState &state, float elevAslM, float groundAslM) {
  /* Stromlos: Block Invalid, letzte Zahl BLEIBT — ein Konsument, der den Kopf ignoriert, darf keine
   * frisch aussehende Null bekommen. */
  if (!Powered_) { state.RadarAlt.H.Invalidate(); return; }
  state.RadarAlt.AglFt = (elevAslM - groundAslM) * kMToFt;
  state.RadarAlt.H.Publish(state.NowS);
}

} // namespace FlightBox::Systems
