#include "FBMissionSim.h"
#include "FBCloudDensity.h"
#include "FBEphemeris.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <limits>

namespace FlightBox::Missions {

const char *FBMissionResultStr(FBMissionResult r) {
  switch (r) {
    case FBMissionResult::Success: return "SUCCESS";
    case FBMissionResult::Fail: return "FAIL";
    case FBMissionResult::Crash: return "CRASH";
    case FBMissionResult::Loc: return "LOC";
    case FBMissionResult::Timeout: return "TIMEOUT";
  }
  return "?";
}

namespace {
/* The one place the two independent monitors' verdicts combine; doc/units-and-missions.md §5. */
FBMissionResult ToMissionResult(FBMissionVerdict v) {
  switch (v) {
    case FBMissionVerdict::Success: return FBMissionResult::Success;
    case FBMissionVerdict::Fail: return FBMissionResult::Fail;
    case FBMissionVerdict::Timeout: return FBMissionResult::Timeout;
    case FBMissionVerdict::None: break;
  }
  return FBMissionResult::Timeout;   /* unreachable in practice: only called once Concluded() */
}

/* IS THIS ACTOR STILL ALIVE — asked of the damage register, so every unit kind answers the same
 * question and no new kind has to be added to a list here. A WEAPON's destruction is its impact and a
 * ground position's is somebody's objective; neither ends the run the cast is flying. */
const Units::FBSimUnit *FirstDestroyed(const Units::FBActorList &actors) {
  for (const auto &a : actors) {
    if (a->GetKind() != Units::FBUnitKind::Aircraft) continue;
    if (a->Health().Destroyed()) return a.get();
  }
  return nullptr;
}

/* Two observed facts plus a declaration, never a team heuristic: a duel has a winner and a loser rather
 * than two failures. Herleitung: doc/units-and-missions.md §5, "Wie aus N Urteilen eines wird". */
bool ExpectedLoss(const Units::FBActorList &actors, const Units::FBSimUnit &a) {
  if (a.Health().CombatEffective()) return false;
  for (const auto &b : actors) {
    if (b.get() == &a) continue;
    const FBMissionMonitor *m = b->MissionMonitor();
    if (!m) continue;
    for (const auto &o : m->Objectives())
      if (FBObjectiveCovers(o, a.GetName().c_str(), a.GetTeam())) return true;
  }
  return false;
}

/* An actor is JUDGED iff the mission gave it objectives; an expected loss does not decide the run. */
const Units::FBSimUnit *FirstDecidingFailure(const Units::FBActorList &actors) {
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (!m || !m->Concluded() || m->Verdict() == FBMissionVerdict::Success) continue;
    if (!ExpectedLoss(actors, *a)) return a.get();
  }
  return nullptr;
}

bool AllJudgedConcluded(const Units::FBActorList &actors) {
  bool anyJudged = false;
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (!m) continue;
    anyJudged = true;
    if (!m->Concluded()) return false;
  }
  return anyJudged;
}

/* Whose verdict the combined RESULT quotes when nothing decided the run: the first judged actor whose
 * loss was NOT somebody's objective — and if every one lost that way (a mutual exchange), the first. */
const Units::FBSimUnit *FirstJudged(const Units::FBActorList &actors) {
  for (const auto &a : actors)
    if (a->MissionMonitor() && !ExpectedLoss(actors, *a)) return a.get();
  for (const auto &a : actors)
    if (a->MissionMonitor()) return a.get();
  return nullptr;
}

} // namespace

FBMissionSim::FBMissionSim(Units::FBActorList &actors, Units::FBUnitRegistry &units,
                           FBOrdnance &ordnance, const FBElevationProvider &elevation,
                           const FBWeatherProvider &weather, double timeoutS)
    : Actors_(actors), Units_(units), Ordnance_(ordnance), Elevation_(elevation), Weather_(&weather),
      TimeoutS_(timeoutS) {
  AnchorLat_ = Actors_.empty() ? 0.0 : Actors_.front()->State().lat;
  AnchorLon_ = Actors_.empty() ? 0.0 : Actors_.front()->State().lon;
  /* The list's CAPACITY is the ceiling its owner computed (declared actors + every loaded station), so
   * nothing index-parallel to it is ever resized while a tick holds a reference into it. */
  RosterBuf_.reserve(Actors_.capacity());
  RosterUnit_.reserve(Actors_.capacity());
}

void FBMissionSim::Prime() {
  for (auto &a : Actors_) a->PrimeState();
}

FBMissionRoster FBMissionSim::BuildRoster() {
  RosterBuf_.clear();
  RosterUnit_.clear();
  for (const auto &a : Actors_) {
    if (a->GetKind() == Units::FBUnitKind::Weapon) continue;   /* a round in the air is nobody's target */
    /* The radiating bit, off the signature the unit publishes at the barrier — the identical
     * construction the health bit and the release bit beside it already use. Nothing is asked of the
     * module and nothing is told to it. */
    bool emitting = false;
    const Units::FBUnitSignature sig = a->GetSignature();
    for (int bi = 0; bi < kMaxEmitterBeams; bi++)
      emitting = emitting || sig.Radar[bi].Mode != FBEmitterMode::None;
    RosterBuf_.push_back({a->GetName().c_str(), a->GetTeam(), a->Health().CombatEffective(),
                          a->ReleasedWeapon(), emitting, std::numeric_limits<double>::infinity()});
    RosterUnit_.push_back(a.get());
  }
  return FBMissionRoster{RosterBuf_.data(), (int)RosterBuf_.size()};
}

/* Aimed FROM the judged unit, out of the published poses, right before its monitor runs. */
void FBMissionSim::AimRoster(const Units::FBSimUnit &self) {
  Units::FBUnitPose p = self.GetPose();
  for (size_t i = 0; i < RosterBuf_.size(); i++) {
    Units::FBUnitPose q = RosterUnit_[i]->GetPose();
    RosterBuf_[i].RangeM = FBPlanarDistM(p.LatDeg, p.LonDeg, q.LatDeg, q.LonDeg);
  }
}

void FBActorStep::operator()(size_t i) const { Sim_.StepActor(i); }

void FBMissionSim::StepActor(size_t i) {
  if (!Actors_[i]->Active()) return;   /* an impacted store integrates no further (FBSimUnit::Retire) */
  FBLogUnitScope us(Actors_[i]->LogLabel());
  Actors_[i]->Run(kSimTickS, &Units_, World_);
}

/* The default STEP is the sequential reference path: actor order, on the caller's thread. */
void FBMissionSim::StepActors() {
  const FBActorStep step(*this);
  if (Stepper_) {
    Stepper_->Step(step, Actors_.size(), SimT_);
    return;
  }
  for (size_t i = 0; i < Actors_.size(); i++) step(i);
}

/* SNAPSHOT DISCIPLINE: the per-actor passes are separate loops on purpose — everything integrates
 * against the poses of the LAST completed tick and only PublishPose (the barrier) makes the new ones
 * visible, so tick ORDER cannot influence the result. That is what lets STEP run one thread per actor
 * while every other pass stays a sequential loop in actor order. Which pass is which and why:
 * doc/units-and-missions.md §7. */
void FBMissionSim::RunPhases() {
  const double dt = kSimTickS;
  /* Ground truth and air mass, both at the DECISION tick and not per 100 Hz substep: a GFS field
   * varies over ~50 km and the jet covers under 60 m in a tick, so a finer sample would be the same
   * number ten times. */
  for (auto &a : Actors_) {
    if (!a->Active()) continue;
    a->UpdateGroundAsl(Elevation_.GroundElevM(a->State().lat, a->State().lon));
    a->UpdateWind(Weather_->WindNedMs(a->State().lat, a->State().lon, a->State().elev));
    /* The cloud decks over this actor, from the SAME sample rate and the same provider as the wind.
     * Nothing reads it unless the module composes a sensor that does (FBModule::SetCloudSky is a
     * no-op by default), so a mission with no weather and no optical sensor is unaffected. */
    a->UpdateSky(FBCloudSkyFromWeather(*Weather_, a->State().lat, a->State().lon, SimT_,
                                       AnchorLat_, AnchorLon_));
    /* The sky's two LIGHTS, from the mission clock and this actor's own position, on the same tick as
     * the decks. Skipped entirely without a declared clock — no clock, no ephemeris, no channel
     * touched (doc/missions/syntax.md). */
    if (Clock_.Have) a->UpdateSolar(FBSolarAt(a->State().lat, a->State().lon, Clock_.At(SimT_)));
  }

  StepActors();
  for (auto &a : Actors_)
    if (a->Active()) a->PublishPose();   /* the barrier: new poses become visible together */

  SimT_ += dt;
  FBLog::SetTime(SimT_);
  FBMissionRoster roster = BuildRoster();
  for (auto &a : Actors_) {
    if (!a->Active()) continue;
    FBLogUnitScope us(a->LogLabel());
    a->CheckEnvelope();   /* generic envelope diagnostics — per actor, not per run */
    if (RangeAware_) AimRoster(*a);
    a->RunMonitors(SimT_, roster);
  }
  for (auto &a : Actors_)
    if (a->Active()) a->SampleTelemetry(SimT_);

  /* THE SAME THREE PHASES IN THIS ORDER, because the order is the semantics: fly and resolve what was
   * already in the air, then let this tick's releases become units, then snapshot the poses the next
   * closest-approach is measured over. A round is therefore never resolved in the tick it left the
   * rail. The hook sits between them exactly where the picture belongs: after the barrier and this
   * tick's telemetry, before the list can grow. */
  Ordnance_.Resolve(Actors_, SimT_, dt);
  if (Hook_) Hook_->OnTick(Actors_, SimT_);
  Ordnance_.Launch(Actors_, Units_, SimT_);
  Ordnance_.SnapPoses(Actors_);   /* after the growth, so an actor that appeared this tick has an entry */
}

/* The trailing timeout is the backstop for a cast with no objectives at all; every judged actor's own
 * monitor concludes TIMEOUT at exactly this sim time. */
bool FBMissionSim::Alive() const {
  return !FirstDestroyed(Actors_) && !FirstDecidingFailure(Actors_) && !AllJudgedConcluded(Actors_) &&
         SimT_ < TimeoutS_;
}

FBRunState FBMissionSim::Tick() {
  if (Concluded_) return FBRunState::Concluded;
  if (!Alive()) { Conclude(); return FBRunState::Concluded; }
  RunPhases();
  if (!Alive()) { Conclude(); return FBRunState::Concluded; }
  return FBRunState::Running;
}

FBRunState FBMissionSim::Advance(double budgetS) {
  if (Concluded_) return FBRunState::Concluded;
  AccS_ += budgetS;
  while (AccS_ >= kSimTickS) {
    if (Tick() == FBRunState::Concluded) return FBRunState::Concluded;
    AccS_ -= kSimTickS;
  }
  return FBRunState::Running;
}

FBMissionResult FBMissionSim::RunToConclusion() {
  while (Tick() == FBRunState::Running) {}
  return Result_;
}

/* Step 4: validate the world — the monitors already did; this combines their verdicts. */
void FBMissionSim::Conclude() {
  Concluded_ = true;
  const Units::FBSimUnit *ko = FirstDestroyed(Actors_);
  auto finalizeAll = [&]() {
    FBMissionRoster roster = BuildRoster();
    for (auto &a : Actors_) {
      FBLogUnitScope us(a->LogLabel());
      a->FinalizeMission(SimT_, roster);
    }
  };
  /* A K.O. always ENDS the run but only DECIDES it when it was nobody's declared objective. When it is
   * expected, the monitors still waiting on a `survive` are asked here — the run to survive is over,
   * and their verdicts are part of the combination below. */
  if (ko && ExpectedLoss(Actors_, *ko)) {
    finalizeAll();
    ko = nullptr;
  }
  const Units::FBSimUnit *failed = ko ? nullptr : FirstDecidingFailure(Actors_);
  Deciding_ = ko ? ko : failed;
  const Units::FBSimUnit *judged = FirstJudged(Actors_);
  if (ko) {
    /* WHAT killed it is the physical judge's statement, quoted here — the register says THAT it is
     * destroyed, the observer says why. */
    Result_ = ko->FlightMonitor().Reason() == FBKoReason::Loc ? FBMissionResult::Loc
                                                             : FBMissionResult::Crash;
    Reason_ = ko->FlightMonitor().Detail();
  } else if (failed) {
    Result_ = ToMissionResult(failed->MissionMonitor()->Verdict());
    Reason_ = failed->MissionMonitor()->Detail();
  } else if (judged) {
    Result_ = ToMissionResult(judged->MissionMonitor()->Verdict());
    Reason_ = judged->MissionMonitor()->Detail();
  } else {
    Result_ = FBMissionResult::Timeout;   /* no actor carried objectives — only the clock could end this */
    Reason_ = "sim time exceeded the mission timeout";
  }
  /* EVERY OPEN JUDGE IS ASKED BEFORE THE REPORT, whatever ended the run. Without this a run stopped by
   * an unexpected K.O. (or by somebody else's decisive failure) leaves the other monitors unconcluded,
   * so they publish no `mission OBJECTIVE` vector at all and every consumer has to read "never judged"
   * as "nothing met" — which pays a doctrine for keeping the OPPONENT airborne
   * (doc/doctrine-evolution.md X-1). WHEN the run ends is untouched; only whether the judges finish.
   * Deliberately AFTER the combination above: the verdicts that existed when the run ended decide it,
   * and a monitor closing here can therefore not move `ko`, `failed`, `judged` or the result. */
  finalizeAll();
}

int FBMissionSim::ExitCode() const {
  switch (Result_) {
    case FBMissionResult::Success: return 0;
    case FBMissionResult::Fail: return 1;
    case FBMissionResult::Crash: return 2;
    case FBMissionResult::Loc: return 2;   /* shares Crash's exit code — see FBMissionResult's banner */
    case FBMissionResult::Timeout: return 3;
  }
  return 1;
}

} // namespace FlightBox::Missions
