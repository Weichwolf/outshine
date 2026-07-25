/* FlightBox — FBF16Ufc: ICP/DED placeholder (doc/f16/cockpit-displays.md) — the two facts the HUD needs
 * off the UFC/DED that no other system produces yet: the ALOW floor (pilot-set low-altitude warning,
 * MIL-STD-1787 pull-up cue reference) and the selected steerpoint number. Both are fixed, settable
 * placeholders (no DED page logic) until the real ICP is modelled. F-16-specific: the ICP/DED is this
 * airframe's own control head, not a generic module slot. */
#ifndef FBF16UFC_H
#define FBF16UFC_H

#include "FBState.h"

namespace FlightBox {

class FBF16Ufc {
public:
  virtual ~FBF16Ufc() = default;

  void SetAlow(float ft) { AlowFt = ft; }
  void SetSteerpointNumber(int n) { StNum = n; }

  virtual void Run(FBState &state, double dt);

private:
  float AlowFt = 500.0f;   /* MIL-STD-1787 pull-up floor placeholder — see doc/f16/aerodynamics-performance.md */
  int StNum = 1;
};

} // namespace FlightBox
#endif
