/* THE SIMULATION LOOP, and there is exactly one of it. A client does not step a mission — it asks this
 * object to run until it stops or until the client owes somebody a picture, and gets back whether the
 * run is still going. The end rule therefore lives where the loop lives, and a client cannot leave it
 * out because no client writes the loop: Tick() is PRIVATE and FBRunState is [[nodiscard]].
 *
 * That is not a style preference. The browser had written itself a SECOND loop, forgot the end rule in
 * it, and flew a mission into a mountain while the frame loop kept integrating (doc/clients/clients.md
 * §5.6). One tick body, one end rule, two owners — the same cut FBOrdnance already made for what leaves
 * the jet.
 *
 * WHAT ENDS A RUN is asked of the DAMAGE REGISTER (`FBSystemHealth::Destroyed`), not of an aircraft's
 * flight monitor: every unit kind has health, only an aircraft has a stall. The physical judge states,
 * the register records, the loop reads — core/FBFlightMonitor -> FBDamageModel::ApplyPhysicalKo ->
 * FBSystemHealth. doc/units-and-missions.md §5/§7. */
#ifndef FBMISSIONSIM_H
#define FBMISSIONSIM_H

#include <string>
#include <vector>
#include "FBClockBoot.h"
#include "FBElevationProvider.h"
#include "FBMissionMonitor.h"
#include "FBOrdnance.h"
#include "FBSimTick.h"
#include "FBSimUnit.h"
#include "FBSpawn.h"
#include "FBUnitRegistry.h"
#include "FBWeatherProvider.h"

namespace FlightBox::World { class FBWorld; }   /* borrowed by the STEP phase only — never included */

namespace FlightBox::Missions {

/* Loc shares Crash's exit code 2: both are physical K.O.s, distinguished by the RESULT line's `result`
 * field. A caller branching only on exit != 0 — the documented contract — sees no difference. */
enum class [[nodiscard]] FBMissionResult { Success, Fail, Crash, Loc, Timeout };
const char *FBMissionResultStr(FBMissionResult r);

/* [[nodiscard]] on the TYPE, not on one function: every present and future way of advancing a
 * simulation hands this back, and dropping it is the exact mistake this file exists to make
 * impossible. `-Wunused-result` is an error in every one of the four builds. */
enum class [[nodiscard]] FBRunState { Running, Concluded };

class FBMissionTickHook {
public:
  virtual ~FBMissionTickHook() = default;

  /* Once, after every actor spawned and before the tick loop. `spawn` is the PRIMARY actor's
   * declarative IC — not a runway assumption, since an airborne mission has no runway at all. `actors`
   * and `units` are borrowed and valid for the whole run; everything a renderer hook needs hangs off
   * their entries, which is what keeps this interface GPU-type-free. */
  virtual void OnMissionStart(const FBSpawn &spawn, const Units::FBActorList &actors,
                              const Units::FBUnitRegistry &units) {
    (void)spawn; (void)actors; (void)units;
  }

  /* The DATA side of weather, once, before the first tick: the run's atmosphere, borrowed for as long
   * as the run lasts. A renderer asks it for cover, cloud base and wind; it draws nothing by itself and
   * this interface stays GPU-type-free. */
  virtual void OnWeather(const FBWeatherProvider &weather) { (void)weather; }

  /* The run's CLOCK, once, before everything above: `Have == false` means the mission declared none
   * and the hook keeps whatever clock its client already had. A hook that draws a sky needs this
   * before the first frame, which is why it precedes OnMissionStart. */
  virtual void OnClock(const FBMissionClock &clock) { (void)clock; }

  /* After the pose barrier and this tick's telemetry sample, so every pose the hook reads is this
   * tick's. A camera has ONE eye and rides actors[0]; the list is here for everything that is not the
   * camera. The hook sets its own cadence for anything expensive. */
  virtual void OnTick(const Units::FBActorList &actors, double simT) = 0;
};

/* THE ONLY THING A CLIENT'S STEP PHASE MAY DO, and it cannot be manufactured: FBMissionSim hands one
 * out for the duration of one STEP phase and is its only constructor. `i` indexes the cast; the tick's
 * dt, the log attribution and the skip rule for a retired actor are the simulation's and are applied
 * inside. Non-copyable, so it cannot be stashed by value and used between ticks. */
class FBActorStep {
public:
  FBActorStep(const FBActorStep &) = delete;
  FBActorStep &operator=(const FBActorStep &) = delete;
  void operator()(size_t i) const;

private:
  friend class FBMissionSim;
  explicit FBActorStep(FBMissionSim &sim) : Sim_(sim) {}
  FBMissionSim &Sim_;
};

/* The ONE phase of a tick a client may replace, because it is the one whose COST is a client property:
 * fb-gym steps the cast across a thread pool, the browser steps it on the frame thread. Everything
 * else — ground truth, the barrier, the judges, ordnance — is the simulation's and not negotiable.
 * A stepper decides WHERE and IN WHICH ORDER `step(i)` runs for i < count, and nothing else. */
class FBActorStepper {
public:
  virtual ~FBActorStepper() = default;
  virtual void Step(const FBActorStep &step, size_t count, double simT) = 0;
};

class FBMissionSim {
public:
  /* Everything is BORROWED and must outlive the run: the cast, its registry, the ordnance book, the
   * ground truth and the air mass. `timeoutS` is the mission's own, the backstop for a cast with no
   * objectives at all. */
  FBMissionSim(Units::FBActorList &actors, Units::FBUnitRegistry &units, FBOrdnance &ordnance,
               const FBElevationProvider &elevation, const FBWeatherProvider &weather, double timeoutS);

  /* Re-pointing the air mass, because the browser's live /wx can land mid-session; it is adopted at
   * the top of a frame and never between two substeps. */
  void SetWeather(const FBWeatherProvider &weather) { Weather_ = &weather; }
  void SetClock(const FBMissionClock &clock) { Clock_ = clock; }
  /* Only an `identify` objective reads a range, so the ranges are only computed for a mission that
   * declares one — the roster every other mission sees is entry for entry the one it saw before. */
  void SetRangeAware(bool on) { RangeAware_ = on; }
  void SetHook(FBMissionTickHook *hook) { Hook_ = hook; }
  void SetStepper(FBActorStepper *stepper) { Stepper_ = stepper; }
  /* The renderer's world, for the module slots entitled to it. Null in every headless client. */
  void SetWorld(const World::FBWorld *world) { World_ = world; }

  /* Boot only: one FDM step per actor so a client's first FRAME reads a filled state. Not part of a
   * tick — a client that needs no picture before the first tick never calls it. */
  void Prime();

  /* RUN UNTIL YOU STOP OR UNTIL I OWE SOMEBODY A PICTURE. `budgetS` is the client's own elapsed wall
   * time; what it buys is a whole number of kSimTickS steps and the remainder is carried, so a machine
   * too slow for the tick rate loses sim time and never the outcome of a tick. */
  FBRunState Advance(double budgetS);
  /* The headless form of the same sentence: no picture is owed, so it returns when the run ended —
   * and it returns the VERDICT, because that is the only thing left to know about a finished run. */
  FBMissionResult RunToConclusion();

  double SimTimeS() const { return SimT_; }
  /* Where between two ticks the client's clock stands, 0..1 — the alpha a camera is carried on. */
  double TickPhase() const { return AccS_ / kSimTickS; }

  bool Concluded() const { return Concluded_; }
  FBMissionResult Result() const { return Result_; }
  const std::string &Reason() const { return Reason_; }
  /* WHO decided the run: the destroyed actor, or the one whose objectives failed. Null on a clean
   * success — nobody decided anything alone, so a report carries no unit attribution. */
  const Units::FBSimUnit *Deciding() const { return Deciding_; }
  /* 0/1/2/3 = SUCCESS/FAIL/CRASH|LOC/TIMEOUT, the documented process contract. */
  int ExitCode() const;

private:
  /* THE reason no client can skip the verdict: one tick is not reachable from outside. */
  friend class FBActorStep;
  FBRunState Tick();
  void RunPhases();
  void StepActors();
  void StepActor(size_t i);
  /* The loop predicate, live: nothing is cached, so asking it before and after a tick is the same
   * question the old `while (...)` head asked. */
  bool Alive() const;
  void Conclude();
  FBMissionRoster BuildRoster();
  void AimRoster(const Units::FBSimUnit &self);

  Units::FBActorList &Actors_;
  Units::FBUnitRegistry &Units_;
  FBOrdnance &Ordnance_;
  const FBElevationProvider &Elevation_;
  const FBWeatherProvider *Weather_ = nullptr;
  const World::FBWorld *World_ = nullptr;
  FBMissionTickHook *Hook_ = nullptr;
  FBActorStepper *Stepper_ = nullptr;
  FBMissionClock Clock_;

  double TimeoutS_;
  double SimT_ = 0.0;
  double AccS_ = 0.0;
  /* ONE cloud field for the whole cast: the decks are sampled over each actor, but the horizontal field
   * is measured from a single anchor — the primary actor's spawn — so two aircraft cannot disagree
   * about where the holes in one deck are. core/FBCloudDensity.h, FBCloudSky::AnchorLatDeg. */
  double AnchorLat_ = 0.0, AnchorLon_ = 0.0;
  bool RangeAware_ = false;
  bool Concluded_ = false;

  std::vector<FBUnitObservation> RosterBuf_;
  std::vector<const Units::FBSimUnit *> RosterUnit_;   /* index-parallel: whose pose each entry is */

  FBMissionResult Result_ = FBMissionResult::Timeout;
  std::string Reason_;
  const Units::FBSimUnit *Deciding_ = nullptr;
};

} // namespace FlightBox::Missions
#endif
