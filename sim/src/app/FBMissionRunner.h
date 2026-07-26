/* FlightBox — FBMissionRunner: the mission ORCHESTRATOR both the native --mission runner and the fb-gym
 * client drive — exactly four steps, no mission specifics of its own: (1) load the mission file, (2)
 * set up the world with its actors (elevation-resolve the declarative spawn, spawn each actor as one
 * units/FBSimUnit, wire its telemetry), (3) execute the actors (step each unit + feed its two monitors
 * at a fixed 10 Hz decision tick), (4) validate the world (the monitors' verdicts become the exit
 * code). Steps 2-4 are LOOPS over a list of actors that today holds exactly one element — a property of
 * today's mission file (one actor block, core/FBMissionFile.h), not of this runner. ALL judgement —
 * physical K.O. (core/FBFlightMonitor) and mission verdict (core/FBMissionMonitor: waypoints/off-runway/
 * timeout) — lives in those two core/ classes, per actor, never inline here. Ground truth comes from an
 * injected FBElevationProvider (the elevation hook), not a hard fb-tiles wire, so this file has NO
 * renderer/world/Dawn dependency and is part of the core library gym links. A caller that wants MORE
 * than headless telemetry (the native frame-oracle's --interval PNGs) supplies an FBMissionTickHook —
 * its interface is deliberately GPU-type-free so this header stays linkable into fb-gym;
 * FBAppNative.cpp implements the concrete hook that owns FBRenderer/FBWorld in ITS OWN translation
 * unit, never in this one. */
#ifndef FBMISSIONRUNNER_H
#define FBMISSIONRUNNER_H
#include <string>
#include "FBElevationProvider.h"
#include "FBSpawn.h"
#include "FBSimUnit.h"

namespace FlightBox {

/* Loc shares Crash's exit code (2) in FBRunMission below — both are FBFlightMonitor K.O. terminations,
 * distinguished by the RESULT log line's `result` field (LOC vs CRASH), not by exit code; a caller that
 * only branches on exit != 0 (the documented contract) sees no difference. */
enum class FBMissionResult { Success, Fail, Crash, Loc, Timeout };
const char *FBMissionResultStr(FBMissionResult r);

class FBMissionTickHook {
public:
  virtual ~FBMissionTickHook() = default;

  /* Called once, right after the spawn succeeds and before the tick loop starts — the hook's chance to
   * set up whatever it wants (native: GPU device + terrain streaming, warmed at `spawn`/the actor's
   * ground ASL — the declarative IC, not a runway assumption: an airborne-only mission has no runway at
   * all). `primary` is the first actor, read-only and valid for the whole run: everything a renderer
   * hook needs (pose, ground/AGL, HUD state, the module's Displays slot for FBRenderer::SetHudDisplay)
   * hangs off it, which is why this interface stays GPU-type-free. */
  virtual void OnMissionStart(const FBSpawn &spawn, const FBSimUnit &primary) {
    (void)spawn; (void)primary;
  }

  /* Called once per 10 Hz decision tick, after this tick's telemetry Bus sample. A camera has ONE eye,
   * so the hook follows the PRIMARY actor (the first unit in the runner's list) — a multi-unit mission
   * that wants per-unit frames adds a per-unit call here, it does not change what "primary" means. The
   * hook decides its own cadence for anything expensive (native throttles PNG dumps to --interval). */
  virtual void OnTick(const FBSimUnit &primary, double simT) = 0;
};

/* mkdir -p `dir` (creates every missing path component). Shared by every app main() that takes --out. */
bool FBEnsureDir(const std::string &dir);

/* Ground-spawns `missionPath`'s module (elevation from `elevation`, aircraft data at `aircraftPath` —
 * native: "vendor/jsbsim/aircraft") and steps it headless at 10 Hz until SUCCESS/CRASH/TIMEOUT/FAIL,
 * writing outDir/telemetry.csv + outDir/events.log (installs its own FBLog sink: file + stdout).
 * `timeoutOverride` > 0 overrides the mission file's own timeout. `hook` (optional) is polled once at
 * spawn and once per tick — see FBMissionTickHook above. Returns 0/1/2/3 = Success/Fail/Crash/Timeout. */
int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const std::string &aircraftPath, FBElevationProvider &elevation,
                 FBMissionTickHook *hook = nullptr);

} // namespace FlightBox
#endif
