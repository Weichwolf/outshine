/* FlightBox — FBFdmBoot: the ONE entry point that applies an initial condition and produces a loaded,
 * trimmed FBFdm. A SEPARATE header on purpose — reaching the IC requires naming THIS file, which only
 * app/ does, so including FBFdm.h grants no way to place, trim or re-spawn an airframe. */
#ifndef FBFDMBOOT_H
#define FBFDMBOOT_H

#include <memory>
#include <string>
#include "FBFdm.h"

namespace FlightBox::Fdm {

/* The initial condition, as data (core/FBSpawn is the mission-level declaration this is resolved into). */
struct FBFdmSpawn {
  std::string ModelsRoot;      /* the ONE model root (app/FBModelRoots.h) */
  std::string Aircraft;        /* model directory + xml name under ModelsRoot */
  double LatDeg = 0.0;         /* GEODETIC — matches GPS/HOME_LAT */
  double LonDeg = 0.0;
  double GroundElevM = 0.0;    /* resolved ground under the spawn point, m ASL */
  double HeightOffsetM = -1.0; /* < 0 = sit on the gear at the model's own clearance; >= 0 = metres above
                                * GroundElevM (0 falls back to a provisional 3 m) */
  double SpeedMs = 0.0;        /* calibrated airspeed; 0 = a standing ground start (no trim search) */
  double HeadingDeg = 0.0;
  bool   FbwOverride = false;  /* engage fcs/fbw-override: FlightBox's FCS, not the aircraft's own */

  /* ---- The RELEASE initial condition: the same one IC application with the fields a released object
   * needs — carrier attitude and full velocity VECTOR instead of heading + calibrated speed, and no
   * trim search. Ignored entirely when Ballistic is false. ---- */
  bool   Ballistic = false;
  double PitchDeg = 0.0, RollDeg = 0.0;              /* attitude at separation (yaw = HeadingDeg) */
  double VelNorthMs = 0.0, VelEastMs = 0.0, VelDownMs = 0.0;   /* NED velocity at separation */
};

class FBFdmBoot {
public:
  /* nullptr if the model failed to load — an FBFdm that exists is always a loaded one, so no caller
   * needs a "not initialised" branch. */
  static std::unique_ptr<FBFdm> Spawn(const FBFdmSpawn &spawn);
};

} // namespace FlightBox::Fdm
#endif
