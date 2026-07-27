/* The mission ORCHESTRATOR both the native --mission runner and fb-gym drive: exactly four steps and no
 * mission specifics of its own — load, set up the world with its actors, execute them, validate.
 * ALL judgement lives in core/FBFlightMonitor and core/FBMissionMonitor, per actor; this file only
 * COMBINES their verdicts into one exit code. Ground truth arrives through an injected
 * FBElevationProvider, so this has no renderer/world/Dawn dependency and belongs to the core library.
 * A caller wanting more than headless telemetry supplies an FBMissionTickHook, whose interface is
 * deliberately GPU-type-free. doc/flightbox/units-and-missions.md, Abschnitt 5-7. */
#ifndef FBMISSIONRUNNER_H
#define FBMISSIONRUNNER_H
#include <string>
#include "FBElevationProvider.h"
#include "FBModelRoots.h"
#include "FBSpawn.h"
#include "FBSimUnit.h"
#include "FBUnitRegistry.h"

namespace FlightBox {

/* Loc shares Crash's exit code 2: both are FBFlightMonitor K.O.s, distinguished by the RESULT line's
 * `result` field. A caller branching only on exit != 0 — the documented contract — sees no difference. */
enum class FBMissionResult { Success, Fail, Crash, Loc, Timeout };
const char *FBMissionResultStr(FBMissionResult r);

class FBMissionTickHook {
public:
  virtual ~FBMissionTickHook() = default;

  /* Once, after every actor spawned and before the tick loop. `spawn` is the PRIMARY actor's
   * declarative IC — not a runway assumption, since an airborne mission has no runway at all. `actors`
   * and `units` are borrowed and valid for the whole run; everything a renderer hook needs hangs off
   * their entries, which is what keeps this interface GPU-type-free. */
  virtual void OnMissionStart(const FBSpawn &spawn, const FBActorList &actors,
                              const FBUnitRegistry &units) {
    (void)spawn; (void)actors; (void)units;
  }

  /* After the pose barrier and this tick's telemetry sample, so every pose the hook reads is this
   * tick's. A camera has ONE eye and rides actors[0]; the list is here for everything that is not the
   * camera. The hook sets its own cadence for anything expensive. */
  virtual void OnTick(const FBActorList &actors, double simT) = 0;
};

/* mkdir -p: creates every missing path component. */
bool FBEnsureDir(const std::string &dir);

/* Steps the mission headless at 10 Hz, writing outDir/telemetry.csv + events.log; installs its own
 * FBLog sink. `timeoutOverride` > 0 beats the file's own timeout. Returns 0/1/2/3 =
 * Success/Fail/Crash/Timeout.
 *
 * `threads` sizes the STEP-phase pool INCLUDING the calling thread. No caller can get a different
 * RESULT out of a larger one: everything with a cross-actor reach stays sequential and log output is
 * replayed in actor order at the barrier. Clamped to the actor count. */
int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const FBModelRoots &models, FBElevationProvider &elevation,
                 FBMissionTickHook *hook = nullptr, size_t threads = 1);

} // namespace FlightBox
#endif
