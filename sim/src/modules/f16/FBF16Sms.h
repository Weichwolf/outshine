/* FlightBox — FBF16Sms: Stores Management System placeholder (doc/f16/weapons.md) — today only the
 * master-arm status the HUD's ARM/SIM line reads (FBArmState). F-16-specific: SMS/pylon state is this
 * airframe's own weapons system, not a generic module slot; a real SMS (pylon inventory, release logic)
 * replaces this later without moving the FBState contract. */
#ifndef FBF16SMS_H
#define FBF16SMS_H

#include "FBState.h"

namespace FlightBox {

class FBF16Sms {
public:
  virtual ~FBF16Sms() = default;

  void SetArmState(FBArmState s) { State = s; }

  virtual void Run(FBState &state, double dt);

private:
  FBArmState State = FBArmState::Arm;   /* matches the guide's reference frame (hud-symbology.md, p.706) */
};

} // namespace FlightBox
#endif
