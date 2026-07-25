/* FlightBox — FBRadarAltimeter: AGL from the SAME DEM ground sample the App already fetches for
 * FBRenderer::SetAgl (fb_stream_ground) — this class does not re-query terrain, it converts the App's
 * already-resolved elev/ground pair (m ASL) into the HUD's radar-altitude readout (ft). The DEFAULT
 * implementation of a module's radar-altimeter slot; airframe-agnostic (every module with a CARA reads
 * the same geometry), Run() is the override point for a module whose radalt genuinely differs. */
#ifndef FBRADARALTIMETER_H
#define FBRADARALTIMETER_H

#include "FBState.h"

namespace FlightBox {

class FBRadarAltimeter {
public:
  virtual ~FBRadarAltimeter() = default;

  /* elevAslM/groundAslM: the aircraft's ASL and the DEM ground ASL under it, metres — exactly the pair
   * the App already computes (`ground = fb_stream_ground(...)`) before calling FBRenderer::SetAgl. */
  virtual void Run(FBState &state, float elevAslM, float groundAslM);
};

} // namespace FlightBox
#endif
