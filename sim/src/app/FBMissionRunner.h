/* FlightBox — FBMissionRunner: the headless mission-loop core BOTH the native --mission runner and the
 * fb-gym client drive — ground-spawn (FBMissionBoot.h) + FBPilot's phase machine stepped at a fixed
 * 10 Hz decision tick until SUCCESS/CRASH/TIMEOUT/FAIL, telemetry.csv + events.log written throughout.
 * Ground truth comes from an injected FBElevationProvider (the elevation hook), not a hard fb-tiles
 * wire, so this file has NO renderer/world/Dawn dependency and is part of the core library gym links.
 * A caller that wants MORE than headless telemetry (the native frame-oracle's --interval PNGs) supplies
 * an FBMissionTickHook — its interface is deliberately GPU-type-free so this header stays linkable into
 * fb-gym; FBAppNative.cpp implements the concrete hook that owns FBRenderer/FBWorld in ITS OWN
 * translation unit, never in this one. */
#ifndef FBMISSIONRUNNER_H
#define FBMISSIONRUNNER_H
#include <string>
#include "FBDisplaySystem.h"
#include "FBElevationProvider.h"
#include "FBRunway.h"
#include "FBState.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

/* Loc shares Crash's exit code (2) in FBRunMission below — both are FBFlightMonitor K.O. terminations,
 * distinguished by the RESULT log line's `result` field (LOC vs CRASH), not by exit code; a caller that
 * only branches on exit != 0 (the documented contract) sees no difference. */
enum class FBMissionResult { Success, Fail, Crash, Loc, Timeout };
const char *FBMissionResultStr(FBMissionResult r);

class FBMissionTickHook {
public:
  virtual ~FBMissionTickHook() = default;

  /* Called once, right after ground-spawn succeeds and before the tick loop starts — the hook's chance
   * to set up whatever it wants (native: GPU device + terrain streaming, warmed at `rwy`/`groundAsl`).
   * `displays` is the module's Displays system slot (core/systems, not GPU) — a renderer hook wires it
   * once via FBRenderer::SetHudDisplay; it stays valid for the whole run. */
  virtual void OnMissionStart(const FBRunway &rwy, double groundAsl, const FBDisplaySystem &displays) {
    (void)rwy; (void)groundAsl; (void)displays;
  }

  /* Called once per 10 Hz decision tick, after this tick's telemetry Bus sample. `telemetry` is the
   * module's own FBState (HUD-ready: AirData/RadarAlt/Nav/... already folded in) — passed as a plain
   * core/ value type so this header stays GPU-free even though a renderer hook wants it for FBHudStage.
   * The hook decides its own cadence for anything expensive (native throttles PNG dumps to its own
   * --interval). */
  virtual void OnTick(const fb_fdm_state &st, double simT, double groundAsl, double aglM,
                      const FBState &telemetry) = 0;
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
