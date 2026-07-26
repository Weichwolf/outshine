/* FlightBox — FBF16Pilot: the F-16's pilot skeleton (systems/FBPilot override point). Round A: composes
 * the DEFAULT (Idle, neutral commands) unmodified — this is where the airframe's own numbers/procedures
 * land once the phase machine grows behaviour:
 *   - Takeoff rotation speed by gross weight -> doc/f16/procedures-takeoff-taxi.md's Vr table
 *     (128..198 KIAS across 20,000..44,000 lb).
 *   - Approach: fly a constant ~11° AoA (13° on-speed, <=13° at touchdown) rather than a fixed speed
 *     -> doc/f16/procedures-landing.md. */
#ifndef FBF16PILOT_H
#define FBF16PILOT_H

#include "FBPilot.h"

namespace FlightBox {

class FBF16Pilot : public FBPilot {
public:
  /* No override yet — the default IS the F-16's behaviour for round A (class banner). */
};

} // namespace FlightBox
#endif
