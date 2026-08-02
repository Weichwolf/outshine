/* The mission ORCHESTRATOR both the native --mission runner and fb-gym drive: exactly four steps and no
 * mission specifics of its own — load, set up the world with its actors, execute them, validate.
 * THE EXECUTION AND ITS VERDICT ARE NOT HERE: they are missions/FBMissionSim, the ONE simulation loop —
 * this runner drives it to conclusion, the browser drives the same object frame by frame. What is left
 * here is everything a HEADLESS run is and a browser is not: files, threads, a campaign carry and a
 * process exit code. ALL judgement lives in core/FBFlightMonitor and core/FBMissionMonitor, per actor.
 * Ground truth arrives through an injected FBElevationProvider, so this has no renderer/world/Dawn
 * dependency and belongs to the core library. WEATHER, unlike ground truth, is not injected but
 * DECLARED: it is part of the scenario (app/FBWeatherBoot.h), and both clients here default to calm so
 * a measurement stays reproducible. A caller wanting more than headless telemetry supplies an
 * FBMissionTickHook, whose interface is deliberately GPU-type-free.
 * doc/units-and-missions.md, Abschnitt 5-7. */
#ifndef FBMISSIONRUNNER_H
#define FBMISSIONRUNNER_H
#include <string>
#include "FBCampaignState.h"
#include "FBClockBoot.h"
#include "FBElevationProvider.h"
#include "FBMissionSim.h"
#include "FBModelRoots.h"
#include "FBWeatherProvider.h"
#include "FBSpawn.h"
#include "FBSimUnit.h"
#include "FBUnitRegistry.h"

namespace FlightBox::Missions {

/* mkdir -p: creates every missing path component. */
bool FBEnsureDir(const std::string &dir);

/* What a finished run leaves for the campaign layer. `State` is the carried part and the ONLY thing
 * that reaches the next mission; `ExpendedByKind` is an observation of THIS run that the aggregate
 * report adds up and nobody carries. */
struct FBMissionOutcome {
  FBCampaignState State;
  int ExpendedByKind[kFBStoreKinds] = {};
};

/* The campaign layer's ONE seam into an otherwise unchanged run: what the earlier missions left
 * (borrowed, applied to the parsed mission BEFORE the spawn and logged line by line), the campaign
 * clock for a mission that declares none, and where this run's own outcome goes. A null `carry` is an
 * ordinary standalone mission, byte-identical to one from before this existed — which is what lets a
 * campaign step be re-run as a normal `fb-gym --mission` with the same state file. */
struct FBMissionCarry {
  const FBCampaignState *In = nullptr;
  uint8_t   Mask = kFBCarryAll;
  int64_t   CampaignUtcT0S = 0;
  bool      HaveCampaignTime = false;
  FBMissionOutcome *Out = nullptr;
};

/* Steps the mission headless at 10 Hz, writing outDir/telemetry.csv + events.log; installs its own
 * FBLog sink. `timeoutOverride` > 0 beats the file's own timeout. Returns 0/1/2/3 =
 * Success/Fail/Crash/Timeout.
 *
 * `threads` sizes the STEP-phase pool INCLUDING the calling thread. No caller can get a different
 * RESULT out of a larger one: everything with a cross-actor reach stays sequential and log output is
 * replayed in actor order at the barrier. Clamped to the actor count.
 *
 * `clientClockOverride` says only whether this client's own clock flag (--utc / FB_SIM_UTC) is set;
 * combined with the mission's `time` line it is resolved in FBClockBoot.h, and a contradiction ends
 * the run before the first tick. */
int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const FBModelRoots &models, FBElevationProvider &elevation,
                 FBMissionTickHook *hook = nullptr, size_t threads = 1,
                 bool clientClockOverride = false, const FBMissionCarry *carry = nullptr);

} // namespace FlightBox::Missions
#endif
