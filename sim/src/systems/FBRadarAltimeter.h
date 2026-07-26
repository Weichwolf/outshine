/* FlightBox — FBRadarAltimeter: AGL from the SAME DEM ground sample the App already fetches for
 * FBRenderer::SetAgl (fb_stream_ground) — this class does not re-query terrain, it converts the App's
 * already-resolved elev/ground pair (m ASL) into the radar-altitude readout (ft) it publishes as the
 * FBRadarAltBlock. The DEFAULT implementation of a module's radar-altimeter slot; airframe-agnostic
 * (every module with a CARA reads the same geometry), Run() is the override point for a module whose
 * radalt genuinely differs.
 *
 * IT IS THE BUS'S REFERENCE CASE FOR "Invalid" (core/FBBlockStatus.h). The set is a powered box, and
 * doc/f16/controls-commands.md §6.4 documents what depends on that: the CARA ALOW warning fires only
 * with the radar altimeter powered and transmitting, no matter that the DED accepted the threshold.
 * Unpowered therefore does NOT publish a stale or zeroed altitude — it invalidates the block, and every
 * consumer (the pilot's AGL floor, the HUD's R readout, the warning system) then has to say what it
 * does without one. That is what the three-state head buys; a box that published 0 ft instead would
 * fly the jet into the ground. */
#ifndef FBRADARALTIMETER_H
#define FBRADARALTIMETER_H

#include "FBState.h"

namespace FlightBox {

class FBRadarAltimeter {
public:
  virtual ~FBRadarAltimeter() = default;

  void SetPowered(bool on) { Powered_ = on; }
  bool Powered() const { return Powered_; }

  /* elevAslM/groundAslM: the aircraft's ASL and the DEM ground ASL under it, metres — exactly the pair
   * the App already computes (`ground = fb_stream_ground(...)`) before calling FBRenderer::SetAgl. */
  virtual void Run(FBState &state, float elevAslM, float groundAslM);

private:
  bool Powered_ = true;
};

} // namespace FlightBox
#endif
