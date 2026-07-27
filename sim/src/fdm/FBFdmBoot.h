/* FlightBox — FBFdmBoot: the ONE entry point that applies an initial condition and produces a loaded,
 * trimmed FBFdm. Deliberately a SEPARATE header from FBFdm.h: FBFdm's loading constructor is private
 * and this class is its only friend, so including FBFdm.h (which every module/system does, to hold an
 * `FBFdm&`) grants no way to place, trim or re-spawn an airframe — reaching the IC requires naming THIS
 * header, which only app/ does (FBMissionBoot.h, the App boot paths, the test harnesses). That is
 * CLAUDE.md's "Kein Cheaten" made structural: `grep -rn FBFdmBoot src/systems src/modules` is empty and
 * cannot silently stop being empty.
 *
 * ONE IC application per airframe (the declarative-spawn contract, doc/mission-format.md): Spawn()
 * applies position/attitude/velocity together for BOTH a ground sit and an explicit airborne altitude —
 * there is no second, separate airborne code path, and there is no re-init afterwards. */
#ifndef FBFDMBOOT_H
#define FBFDMBOOT_H

#include <memory>
#include <string>
#include "FBFdm.h"

namespace FlightBox {

/* The initial condition, as data (core/FBSpawn is the mission-level declaration this is resolved into). */
struct FBFdmSpawn {
  std::string ModelsRoot;      /* the ONE model root — native/gym "assets/aircraft", WASM the embedded FS
                                * path "/fb/aircraft" (app/FBModelRoots.h) */
  std::string Aircraft;        /* model directory + xml name under ModelsRoot */
  double LatDeg = 0.0;         /* GEODETIC — matches GPS/HOME_LAT */
  double LonDeg = 0.0;
  double GroundElevM = 0.0;    /* resolved ground under the spawn point, m ASL */
  double HeightOffsetM = -1.0; /* < 0 = sit on the gear at the model's own gear-down clearance;
                                * >= 0 = that many metres above GroundElevM (0 falls back to a safe
                                * provisional 3 m — an airborne IC with no explicit offset) */
  double SpeedMs = 0.0;        /* calibrated airspeed; 0 = a standing ground start (no trim search) */
  double HeadingDeg = 0.0;
  bool   FbwOverride = false;  /* engage fcs/fbw-override so FlightBox's own FCS, not the aircraft's own
                                * flight-control system, is the controller */

  /* ---- The RELEASE initial condition (Ballistic = true): a store leaving a pylon, not an aircraft
   * being placed. Same one IC application as every other spawn — this is not a second code path, it is
   * the same call with the fields a released object needs: the carrier's ATTITUDE and its full VELOCITY
   * VECTOR instead of a heading and a calibrated speed. There is nothing to trim (a bomb has no control
   * to trim with and no engine to balance), so the trim search is skipped and the state stands exactly
   * as handed in. Ignored entirely when Ballistic is false, so an aircraft spawn behaves as it always
   * did. ---- */
  bool   Ballistic = false;
  double PitchDeg = 0.0, RollDeg = 0.0;              /* attitude at separation (yaw = HeadingDeg) */
  double VelNorthMs = 0.0, VelEastMs = 0.0, VelDownMs = 0.0;   /* NED velocity at separation */
};

class FBFdmBoot {
public:
  /* Loads `spawn.Aircraft` (+ its engine/ and Systems/), applies the IC, starts every engine, trims
   * (airborne/rolling ICs only — a V=0 ground start has no aerodynamic trim solution to search for) and
   * returns the ready instance. Returns nullptr if the model failed to load: an FBFdm that exists is
   * always a loaded one, so no caller and no method needs a "not initialised" branch. */
  static std::unique_ptr<FBFdm> Spawn(const FBFdmSpawn &spawn);
};

} // namespace FlightBox
#endif
