/* FlightBox — FBAutopilot: outer guidance. Modes (MANUAL pass-through, LOITER bank-to-circle around
 * a geo centre at target altitude/radius) -> a guidance command the inner FBW (FBFlightControl)
 * tracks. Port of flightctl.h's outer loop; numerics preserved verbatim (equivalence-tested). */
#ifndef FBAUTOPILOT_H
#define FBAUTOPILOT_H

#include "jsbsim_adapter.h"

namespace FlightBox {

enum class FBMode { Manual, Loiter };

struct FBGuidance {
  FBMode Mode;
  double BankCmdDeg;     /* commanded bank (deg), loiter geometry + capture */
  double AltErrM;        /* target alt - current alt (m), unclamped — raw material for BOTH the FLCS's
                          * vs-command (TargetVsMs) and the raw path's pitch-attitude command */
  double TargetVsMs;     /* desired vertical speed (m/s) from the altitude loop */
  double TargetSpeedMs;  /* airspeed to hold */
  double ManualRoll, ManualPitch, ManualYaw, ManualThr;   /* pass-through in Manual */
  double RingDistM;      /* diagnostics: current distance from the loiter centre */
};

class FBAutopilot {
public:
  FBAutopilot();

  /* Loiter target: circle centre (deg), altitude (m ASL), radius (m), dir +1 CW / -1 CCW, speed. */
  void SetLoiter(double lat, double lon, double altM, double radiusM, int dir, double speedMs);
  void SetManual(double roll, double pitch, double yaw, double thr);

  FBGuidance Run(const fb_fdm_state &s);

  FBMode GetMode(void) const { return Mode; }
  double GetTargetAlt(void) const { return AltM; }
  double GetRadius(void) const { return RadiusM; }

  /* gains — public like a config block; defaults = the flown F-16 preset */
  double BankMaxDeg, KHdg, KAlt, KXt, KXti;

private:
  FBMode Mode;
  double LatDeg, LonDeg, AltM, RadiusM, SpeedMs;
  int Dir;
  double MRoll, MPitch, MYaw, MThr;
  double XtIterm;        /* cross-track integrator: closes the last few % of ring radius */
};

} // namespace FlightBox
#endif
