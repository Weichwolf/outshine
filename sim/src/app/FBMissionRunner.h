/* FlightBox — FBMissionRunner: the mission ORCHESTRATOR both the native --mission runner and the fb-gym
 * client drive — exactly four steps, no mission specifics of its own: (1) load the mission file, (2)
 * set up the world with its actors (elevation-resolve the declarative spawn, spawn each actor as one
 * units/FBSimUnit, wire its telemetry), (3) execute the actors (step each unit + feed its two monitors
 * at a fixed 10 Hz decision tick), (4) validate the world (the monitors' verdicts become the exit
 * code). Steps 2-4 are LOOPS over the mission's cast — one actor per `unit` block the .fbm declares
 * (core/FBMissionFile.h), stepped in declaration order behind a pose-publish barrier so tick order
 * cannot change a result. ALL judgement — physical K.O. (core/FBFlightMonitor, ANY actor's K.O. ends
 * the run) and mission verdict (core/FBMissionMonitor: waypoints/off-runway/timeout, one instance per
 * actor WITH objectives; the run succeeds only when every one of them did and fails the moment one
 * does) — lives in those two core/ classes, per actor, never inline here; this file only combines the
 * per-actor verdicts into one exit code. Ground truth comes from an
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

  /* Called once, right after every actor has spawned and before the tick loop starts — the hook's
   * chance to set up whatever it wants (native: GPU device + terrain streaming warmed at `spawn`/the
   * primary actor's ground ASL — the declarative IC, not a runway assumption: an airborne-only mission
   * has no runway at all — plus registering the WHOLE cast in its FBWorld unit registry, which is why
   * this takes the list and not just the primary). `spawn` is the primary actor's declarative IC;
   * `actors` is read-only and valid for the whole run: everything a renderer hook needs (pose,
   * ground/AGL, HUD state, the module's Displays slot for FBRenderer::SetHudDisplay) hangs off its
   * entries, which is why this interface stays GPU-type-free. */
  virtual void OnMissionStart(const FBSpawn &spawn, const FBActorList &actors) {
    (void)spawn; (void)actors;
  }

  /* Called once per 10 Hz decision tick, after the pose-publish barrier and this tick's telemetry Bus
   * samples — so every pose the hook reads is this tick's, for every actor. A camera has ONE eye, so a
   * renderer hook rides actors[0]; the whole list is here because anything BUT the camera (unit
   * markers, a per-unit frame dump) needs the rest. The hook decides its own cadence for anything
   * expensive (native throttles PNG dumps to --interval). */
  virtual void OnTick(const FBActorList &actors, double simT) = 0;
};

/* mkdir -p `dir` (creates every missing path component). Shared by every app main() that takes --out. */
bool FBEnsureDir(const std::string &dir);

/* Ground-spawns `missionPath`'s module (elevation from `elevation`, aircraft data at `aircraftPath` —
 * native: "vendor/jsbsim/aircraft") and steps it headless at 10 Hz until SUCCESS/CRASH/TIMEOUT/FAIL,
 * writing outDir/telemetry.csv + outDir/events.log (installs its own FBLog sink: file + stdout).
 * `timeoutOverride` > 0 overrides the mission file's own timeout. `hook` (optional) is polled once at
 * spawn and once per tick — see FBMissionTickHook above. Returns 0/1/2/3 = Success/Fail/Crash/Timeout.
 *
 * `threads` is the size of the pool that executes the per-actor STEP phase (app/FBTickPool.h), the
 * calling thread included — 1 (the default every caller but fb-gym's `--threads` uses) is the sequential
 * reference path, and no caller can get a different RESULT out of a larger one: everything with a
 * cross-actor reach stays sequential and log output is replayed in actor order at the barrier. It is
 * clamped to the mission's actor count (more threads than actors is idle threads, not speed). */
int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const std::string &aircraftPath, FBElevationProvider &elevation,
                 FBMissionTickHook *hook = nullptr, size_t threads = 1);

} // namespace FlightBox
#endif
