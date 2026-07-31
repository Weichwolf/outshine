#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBEphemeris.h"
#include "FBWeatherBoot.h"
#include "FBModuleRegistry.h"
#include "FBTelemetry.h"
#include "FBTelemetrySinks.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBTickPool.h"
#include "FBGeodesy.h"
#include "FBOrdnance.h"
#include "FBSimTick.h"
#include "FBUnits.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <cmath>
#include <sys/stat.h>
#include <vector>

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

bool FBEnsureDir(const std::string &dir) {
  std::string cur;
  for (size_t i = 0; i <= dir.size(); i++) {
    if (i == dir.size() || dir[i] == '/') {
      if (!cur.empty() && mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return false;
      if (i < dir.size()) cur += '/';
    } else {
      cur += dir[i];
    }
  }
  return true;
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

/* A physical K.O. of ANY aircraft ends the run; returns WHOSE, because that decides the RESULT line. */
const Units::FBSimUnit *FirstFlightKo(const Units::FBActorList &actors) {
  for (const auto &a : actors) {
    if (a->GetKind() != Units::FBUnitKind::Aircraft) continue;   /* a store's K.O. is its impact; a ground
                                                           * target has no flight to lose */
    if (a->FlightMonitor().Tripped()) return a.get();
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

/* One CSV per actor, the primary keeping the canonical name; doc/units-and-missions.md §10. */
std::string TelemetryPath(const std::string &outDir, size_t index, const Units::FBSimUnit &unit) {
  if (index == 0) return outDir + "/telemetry.csv";
  return outDir + "/telemetry_" + unit.GetName() + ".csv";
}

/* Was the ground the cause or the consequence? The shootdown explains the crash, the crash explains
 * nothing — so the physical judge steps aside for a concluded, combat-ineffective unit. */
bool ShotDownFirst(const Units::FBSimUnit &a) {
  const FBMissionMonitor *m = a.MissionMonitor();
  return m && m->Concluded() && !a.Health().CombatEffective();
}

/* The per-unit breakdown's result string: the physical judge outranks the mission judge except after a
 * shootdown. Table: doc/units-and-missions.md §5, "UNIT_RESULT". */
const char *ActorResultStr(const Units::FBSimUnit &a) {
  if (a.GetKind() == Units::FBUnitKind::Weapon)
    return a.FlightMonitor().Tripped() ? "IMPACT" : "IN_FLIGHT";
  if (a.GetKind() == Units::FBUnitKind::Ground)
    return a.Health().CombatEffective() ? "INTACT" : "DESTROYED";
  const FBMissionMonitor *m = a.MissionMonitor();
  if (a.FlightMonitor().Tripped() && !ShotDownFirst(a))
    return a.FlightMonitor().Reason() == FBKoReason::Loc ? "LOC" : "CRASH";
  return m ? FBMissionVerdictStr(m->Verdict()) : "NONE";
}
std::string ActorReason(const Units::FBSimUnit &a) {
  if (a.GetKind() == Units::FBUnitKind::Ground) {
    if (a.Health().CombatEffective()) return "still standing";
    char buf[64];
    snprintf(buf, sizeof buf, "structure destroyed by %d hit(s)", a.Health().Hits());
    return buf;
  }
  if (a.FlightMonitor().Tripped() && !ShotDownFirst(a)) return a.FlightMonitor().Detail();
  const FBMissionMonitor *m = a.MissionMonitor();
  if (!m) return "no objectives";
  return m->Concluded() ? m->Detail() : "still under way when the run ended";
}

/* The file + sink behind one actor's telemetry bus: app/ owns the I/O, the unit owns the bus. */
struct FBActorTelemetry {
  Clients::FBFileHandle File{nullptr, &fclose};
  std::unique_ptr<Clients::FBCsvTelemetrySink> Sink;
};

/* The tick's STEP phase as one job (app/FBTickPool.h): index i steps actor i and nothing else. The sim
 * time is stamped inside RunIndex because FBLog's clock is thread-local. */
class FBActorStepJob : public FBTickJob {
public:
  FBActorStepJob(Units::FBActorList &actors, const Units::FBUnitRegistry &units,
                 std::vector<Clients::FBBufferedLogSink> &logs, double dt)
      : Actors_(actors), Units_(units), Logs_(logs), Dt_(dt) {}

  void SetTime(double simT) { TimeS_ = simT; }

  void RunIndex(size_t i) override {
    if (!Actors_[i]->Active()) return;   /* an impacted store integrates no further (FBSimUnit::Retire) */
    FBLog::SetTime(TimeS_);
    FBLogThreadSinkScope capture(&Logs_[i]);
    FBLogUnitScope us(Actors_[i]->LogLabel());
    Actors_[i]->Run(Dt_, &Units_, nullptr);
  }

private:
  Units::FBActorList &Actors_;
  const Units::FBUnitRegistry &Units_;
  std::vector<Clients::FBBufferedLogSink> &Logs_;
  double Dt_;
  double TimeS_ = 0.0;
};
} // namespace

int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const FBModelRoots &models, FBElevationProvider &elevation,
                 FBMissionTickHook *hook, size_t threads, bool clientClockOverride,
                 const FBMissionCarry *carry) {
  std::string evPath = outDir + "/events.log";
  Clients::FBFileHandle evf = Clients::FBOpenFile(evPath.c_str(), "w");
  if (!evf) { fprintf(stderr, "mission: cannot open %s for writing\n", evPath.c_str()); return 1; }

  /* Declaration order IS the cleanup contract: the scope is declared last, so FBLog's sink pointer is
   * cleared before the sinks and the FILE* behind them go away — on EVERY return below. */
  Clients::FBFileLogSink fileSink(evf.get());
  Clients::FBStdoutLogSink stdoutSink;
  Clients::FBCompositeLogSink logSink;
  logSink.Add(&fileSink);
  logSink.Add(&stdoutSink);
  Clients::FBLogSinkScope logScope(&logSink);
  FBLog::SetTime(0.0);

  /* ---- Step 1: load the mission ---- */
  std::ifstream in(missionPath);
  if (!in) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "cannot open " + missionPath}});
    return 1;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  FBMission mission;
  std::string perr;
  if (!FBParseMissionFile(buf.str(), mission, &perr)) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "parse: " + perr}});
    return 1;
  }
  double timeoutS = timeoutOverride > 0.0 ? timeoutOverride : mission.TimeoutS;
  FBLog::Info("mission", "MISSION_START", {{"name", mission.Name}, {"timeout", timeoutS}});

  /* ---- The campaign carry, between the parse and everything else ----
   * It may only take away (core/FBCampaignState.h), and every removal is logged here so the EFFECTIVE
   * mission is reconstructible from this file alone. A mission run standalone sees none of this. */
  if (carry && carry->In) {
    std::vector<FBCarryChange> changes;
    FBApplyCampaignCarry(*carry->In, carry->Mask, mission, changes);
    for (const FBCarryChange &c : changes) {
      if (c.What == FBCarryChange::Action::DropUnit)
        FBLog::Info("campaign", "CARRY", {{"unit", c.UnitId}, {"action", "drop"},
            {"reason", "destroyed in an earlier mission"}});
      else
        FBLog::Info("campaign", "CARRY", {{"unit", c.UnitId}, {"action", "stores"},
            {"store", c.Store}, {"station", c.Station}, {"reason", "expended in an earlier mission"}});
    }
    if (mission.Units.empty()) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"},
          {"reason", "the campaign carry removed every unit block — this mission has no cast left"}});
      return 1;
    }
  }

  /* The run's clock. Only a DECLARED one produces a line — a mission without `time` has no clock and
   * its events.log stays byte-identical to one from before the clock existed. The sun elevation is
   * logged with it because the file may only say Zulu: this is where "22:00Z over Batajnica is night"
   * becomes checkable without the author doing spherical trigonometry. */
  FBMissionClock clock;
  std::string cerr;
  if (!FBResolveMissionClock(mission, clientClockOverride, clock, &cerr,
                             carry ? carry->CampaignUtcT0S : 0, carry && carry->HaveCampaignTime)) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", cerr}});
    return 1;
  }
  if (clock.Have) {
    char iso[21];
    const FBSpawn &sp0 = mission.Units.front().Spawn;
    const FBSolar s0 = FBSolarAt(sp0.LatDeg, sp0.LonDeg, clock.At(0.0));
    /* The instant is logged as TEXT only: the log's numeric fields are %g, and 9.22313e+08 is a Unix
     * second rounded past the hour it names. */
    FBLog::Info("mission", "CLOCK", {{"utc", FBFormatIsoUtc(clock.T0S, iso, sizeof iso)},
        {"sunElDeg", (double)s0.SunElDeg}, {"sunAzDeg", (double)s0.SunAzDeg},
        {"moonElDeg", (double)s0.MoonElDeg}, {"moonPhase", (double)s0.MoonPhase}});
  }
  if (hook) hook->OnClock(clock);

  /* The mission's atmosphere, or still air. A DECLARED fixture that will not load is a FAIL and not a
   * quiet fallback: a run measured in the wrong weather is worse than no run. */
  std::string werr;
  std::unique_ptr<FBWeatherProvider> weather = FBMakeMissionWeather(mission, models, &werr);
  if (!weather) {
    if (!werr.empty()) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", werr}});
      return 1;
    }
    weather = std::make_unique<FBCalmWeather>();
  }
  if (mission.HaveWeather) {
    /* Only when DECLARED: a calm run's events.log must stay byte-identical to one from before weather. */
    FBLog::Info("mission", "WEATHER", {{"kind", mission.Weather.Kind == FBWeatherKind::Fixture ? "fixture"
                                              : mission.Weather.Kind == FBWeatherKind::Wind ? "wind" : "calm"},
        {"fixture", mission.Weather.Fixture}, {"fromDeg", mission.Weather.WindFromDeg},
        {"speedKt", mission.Weather.WindSpeedKt}});
  }

  /* ---- Step 2: set up the world with its actors ----
   * The list's ORDER is the mission file's order and stays the tick order for the whole run. */
  Modules::FBRegisterBuiltinModules();
  Units::FBActorList Actors;
  Actors.reserve(mission.Units.size());
  for (size_t i = 0; i < mission.Units.size(); i++) {
    const FBMissionUnit &block = mission.Units[i];
    const FBSpawn &sp = block.Spawn;
    /* The unit does not exist yet, so the attribution rule is read from the mission here and from
     * FBSimUnit::LogLabel in every loop below. */
    FBLogUnitScope us(mission.Units.size() > 1 ? block.Id : std::string());
    double groundAsl = elevation.GroundElevM(sp.LatDeg, sp.LonDeg);
    if (!FBElevationResolved(groundAsl)) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "elevation unresolved at spawn"}});
      return 1;
    }
    /* An explicit altitude below the resolved terrain is a contradiction, not an unusual declaration;
     * the 1 m margin absorbs elevation-source rounding, not real penetration. */
    if (!sp.Ground && sp.AltM < groundAsl - 1.0) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "spawn altitude is below ground"},
          {"altM", sp.AltM}, {"groundM", groundAsl}});
      return 1;
    }
    /* `name` stays the MISSION's; WHICH actor this is comes from the `unit=` scope above. */
    FBLog::Info("mission", "SPAWN", {{"name", mission.Name}, {"lat", sp.LatDeg}, {"lon", sp.LonDeg},
        {"ground", sp.Ground}, {"altM", sp.Ground ? groundAsl : sp.AltM}, {"groundAsl", groundAsl},
        {"hdg", sp.HeadingDeg}, {"speedKt", sp.SpeedKt}});

    std::string serr;
    std::unique_ptr<Units::FBSimUnit> unit = FBMissionSpawnActor(models, mission, i, groundAsl, timeoutS, &serr);
    if (!unit) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", serr}});
      return 1;
    }
    Actors.push_back(std::move(unit));
  }

  /* The exact ceiling now that every loadout is applied: one further actor per loaded station and not
   * one more — a store can be released once. Everything index-parallel below is sized for it, so no
   * store appearing mid-run ever resizes a buffer a worker thread is holding a reference to. */
  size_t maxActors = Actors.size();
  for (const auto &a : Actors) maxActors += (size_t)a->Module().MaxReleases();
  Actors.reserve(maxActors);
  /* Everything a round does after it has left the jet, and the browser drives the same object
   * (missions/FBOrdnance.h) — one apparatus, two owners. */
  FBOrdnance Ordnance(models);
  Ordnance.Reserve(maxActors);

  /* The run's ONE unit registry, in mission-declaration order, borrowed by everything that observes
   * units: the modules' sensors through the step job, and (native only) the hook's FBWorld. */
  Units::FBUnitRegistry UnitReg;
  for (const auto &a : Actors) UnitReg.Register(a.get());

  /* The OBSERVED roster a combat objective is judged against: callsign, faction, and the one bit the
   * actor's own health register publishes. Weapons are out — a round in the air is not somebody's
   * target. The Id borrows the unit's own name, which outlives the run. */
  std::vector<FBUnitObservation> RosterBuf;
  std::vector<const Units::FBSimUnit *> RosterUnit;   /* index-parallel: whose pose each entry is */
  RosterBuf.reserve(maxActors);
  RosterUnit.reserve(maxActors);
  auto BuildRoster = [&]() {
    RosterBuf.clear();
    RosterUnit.clear();
    for (const auto &a : Actors) {
      if (a->GetKind() == Units::FBUnitKind::Weapon) continue;
      /* The radiating bit, off the signature the unit publishes at the barrier — the identical
       * construction the health bit and the release bit beside it already use. Nothing is asked of the
       * module and nothing is told to it. */
      bool emitting = false;
      const Units::FBUnitSignature sig = a->GetSignature();
      for (int bi = 0; bi < kMaxEmitterBeams; bi++)
        emitting = emitting || sig.Radar[bi].Mode != FBEmitterMode::None;
      RosterBuf.push_back({a->GetName().c_str(), a->GetTeam(), a->Health().CombatEffective(),
                           a->ReleasedWeapon(), emitting, std::numeric_limits<double>::infinity()});
      RosterUnit.push_back(a.get());
    }
    return FBMissionRoster{RosterBuf.data(), (int)RosterBuf.size()};
  };
  /* `identify` is the only kind that reads a range, so the ranges are only computed for a mission that
   * declares one — the roster a mission without one sees is the one it saw before this round, entry for
   * entry. Aimed FROM the judged unit, out of the published poses, right before its monitor runs. */
  bool NeedRanges = false;
  for (const auto &u : mission.Units)
    for (const auto &o : u.Objectives) NeedRanges = NeedRanges || o.Kind == FBObjectiveKind::Identify;
  auto AimRoster = [&](const Units::FBSimUnit &self) {
    Units::FBUnitPose p = self.GetPose();
    for (size_t i = 0; i < RosterBuf.size(); i++) {
      Units::FBUnitPose q = RosterUnit[i]->GetPose();
      RosterBuf[i].RangeM = FBPlanarDistM(p.LatDeg, p.LonDeg, q.LatDeg, q.LonDeg);
    }
  };

  if (hook) {
    hook->OnWeather(*weather);
    hook->OnMissionStart(mission.Units.front().Spawn, Actors, UnitReg);
  }

  std::vector<FBActorTelemetry> ActorTelemetry;
  ActorTelemetry.reserve(maxActors);
  ActorTelemetry.resize(Actors.size());
  for (size_t i = 0; i < Actors.size(); i++) {
    std::string path = TelemetryPath(outDir, i, *Actors[i]);
    ActorTelemetry[i].File = Clients::FBOpenFile(path.c_str(), "w");
    if (!ActorTelemetry[i].File) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "cannot open telemetry.csv"}});
      return 1;
    }
    ActorTelemetry[i].Sink = std::make_unique<Clients::FBCsvTelemetrySink>(ActorTelemetry[i].File.get());
    Actors[i]->StartTelemetry(ActorTelemetry[i].Sink.get());
  }
  /* A store gets its own trace the moment it becomes a unit; a failed open still flies the round and
   * only loses its trace. This is the ONE thing about a release that is the runner's and not the
   * ordnance's — the browser has no file system to do it in. */
  Ordnance.OnStoreSpawned([&](Units::FBSimUnit &store, size_t index) {
    std::string path = TelemetryPath(outDir, index, store);
    ActorTelemetry.emplace_back();
    FBActorTelemetry &tel = ActorTelemetry.back();
    tel.File = Clients::FBOpenFile(path.c_str(), "w");
    if (tel.File) {
      tel.Sink = std::make_unique<Clients::FBCsvTelemetrySink>(tel.File.get());
      store.StartTelemetry(tel.Sink.get());
    } else {
      store.StartTelemetry(nullptr);
    }
  });

  /* ---- Step 3: execute the actors ---- */
  /* ONE cloud field for the whole cast. The decks are still sampled over each actor (that is its own
   * weather), but the horizontal field is measured from a single anchor — the primary actor's spawn —
   * so two aircraft cannot disagree about where the holes in one deck are. core/FBCloudDensity.h,
   * FBCloudSky::AnchorLatDeg. */
  const double skyAnchorLat = Actors.empty() ? 0.0 : Actors.front()->State().lat;
  const double skyAnchorLon = Actors.empty() ? 0.0 : Actors.front()->State().lon;
  const double dt = kSimTickS;
  double simT = 0.0;
  /* steady_clock, not clock(): the latter sums every thread's CPU time and would report a FASTER
   * parallel run as a slower one. */
  auto wallStart = std::chrono::steady_clock::now();

  /* Nothing about the pool is LOGGED: how many threads stepped the cast is a property of the client and
   * a line about it would be the one difference between a sequential and a parallel events.log. The
   * pool is declared LAST, so it is destroyed FIRST — its threads are joined while the buffers and the
   * job they were handed are still alive. */
  if (threads < 1) threads = 1;
  if (threads > Actors.size()) threads = Actors.size();
  std::vector<Clients::FBBufferedLogSink> actorLogs(maxActors);
  FBActorStepJob stepJob(Actors, UnitReg, actorLogs, dt);
  FBTickPool pool(threads);

  /* SNAPSHOT DISCIPLINE: the per-actor passes are separate loops on purpose — everything integrates
   * against the poses of the LAST completed tick and only PublishPose (the barrier) makes the new ones
   * visible, so tick ORDER cannot influence the result. That is what lets STEP run one thread per actor
   * while every other pass stays a sequential loop in actor order. Which pass is which and why:
   * doc/units-and-missions.md §7. The trailing timeout is the backstop for a cast with no
   * objectives at all; every judged actor's own monitor concludes TIMEOUT at exactly this sim time. */
  while (!FirstFlightKo(Actors) && !FirstDecidingFailure(Actors) && !AllJudgedConcluded(Actors) &&
         simT < timeoutS) {
    /* Ground truth and air mass, both at the DECISION tick and not per 100 Hz substep: a GFS field
     * varies over ~50 km and the jet covers under 60 m in a tick, so a finer sample would be the same
     * number ten times. */
    for (auto &a : Actors) {
      if (!a->Active()) continue;
      a->UpdateGroundAsl(elevation.GroundElevM(a->State().lat, a->State().lon));
      a->UpdateWind(weather->WindNedMs(a->State().lat, a->State().lon, a->State().elev));
      /* The cloud decks over this actor, from the SAME sample rate and the same provider as the wind.
       * Nothing reads it unless the module composes a sensor that does (FBModule::SetCloudSky is a
       * no-op by default), so a mission with no weather and no optical sensor is unaffected. */
      a->UpdateSky(FBCloudSkyFromWeather(*weather, a->State().lat, a->State().lon, simT,
                                         skyAnchorLat, skyAnchorLon));
      /* The sky's two LIGHTS, from the mission clock and this actor's own position, on the same tick
       * as the decks. Skipped entirely without a declared clock — no clock, no ephemeris, no channel
       * touched (doc/missions/syntax.md). */
      if (clock.Have)
        a->UpdateSolar(FBSolarAt(a->State().lat, a->State().lon, clock.At(simT)));
    }
    stepJob.SetTime(simT);
    pool.RunTick(stepJob, Actors.size());
    for (auto &l : actorLogs) l.Drain(logSink);
    for (auto &a : Actors)
      if (a->Active()) a->PublishPose();   /* the barrier: new poses become visible together */
    simT += dt;
    FBLog::SetTime(simT);
    FBMissionRoster roster = BuildRoster();
    for (auto &a : Actors) {
      if (!a->Active()) continue;
      FBLogUnitScope us(a->LogLabel());
      a->CheckEnvelope();   /* generic envelope diagnostics — per actor, not per run */
      if (NeedRanges) AimRoster(*a);
      a->RunMonitors(simT, roster);
    }
    for (auto &a : Actors)
      if (a->Active()) a->SampleTelemetry(simT);
    Ordnance.Resolve(Actors, simT, dt);
    if (hook) hook->OnTick(Actors, simT);

    Ordnance.Launch(Actors, UnitReg, simT);
    /* After the growth above, so an actor that appeared this tick has an entry from now on. */
    Ordnance.SnapPoses(Actors);
  }

  /* ---- Step 4: validate the world — the monitors already did; combine their verdicts ---- */
  const Units::FBSimUnit &primary = *Actors.front();
  const Units::FBSimUnit *ko = FirstFlightKo(Actors);
  auto finalizeAll = [&]() {
    FBMissionRoster roster = BuildRoster();
    for (auto &a : Actors) {
      FBLogUnitScope us(a->LogLabel());
      a->FinalizeMission(simT, roster);
    }
  };
  /* A K.O. always ENDS the run but only DECIDES it when it was nobody's declared objective. When it is
   * expected, the monitors still waiting on a `survive` are asked here — the run to survive is over,
   * and their verdicts are part of the combination below. */
  if (ko && ExpectedLoss(Actors, *ko)) {
    finalizeAll();
    ko = nullptr;
  }
  const Units::FBSimUnit *failed = ko ? nullptr : FirstDecidingFailure(Actors);
  /* Null on a clean success: nobody decided anything alone, so RESULT carries no unit attribution. */
  const Units::FBSimUnit *deciding = ko ? ko : failed;
  const Units::FBSimUnit *judged = FirstJudged(Actors);
  FBMissionResult result;
  std::string reason;
  if (ko) {
    result = ko->FlightMonitor().Reason() == FBKoReason::Loc ? FBMissionResult::Loc : FBMissionResult::Crash;
    reason = ko->FlightMonitor().Detail();
  } else if (failed) {
    result = ToMissionResult(failed->MissionMonitor()->Verdict());
    reason = failed->MissionMonitor()->Detail();
  } else if (judged) {
    result = ToMissionResult(judged->MissionMonitor()->Verdict());
    reason = judged->MissionMonitor()->Detail();
  } else {
    result = FBMissionResult::Timeout;   /* no actor carried objectives — only the clock could end this */
    reason = "sim time exceeded the mission timeout";
  }
  /* EVERY OPEN JUDGE IS ASKED BEFORE THE REPORT, whatever ended the run. Without this a run stopped by
   * an unexpected K.O. (or by somebody else's decisive failure) leaves the other monitors unconcluded,
   * so they publish no `mission OBJECTIVE` vector at all and every consumer has to read "never judged"
   * as "nothing met" — which pays a doctrine for keeping the OPPONENT airborne
   * (doc/doctrine-evolution.md X-1). WHEN the run ends is untouched; only whether the judges finish.
   * Deliberately AFTER the combination above: the verdicts that existed when the run ended decide it,
   * and a monitor closing here can therefore not move `ko`, `failed`, `judged` or `result`. */
  finalizeAll();
  const Fdm::fb_fdm_state &st = primary.State();

  /* With a single actor the RESULT line below IS that actor's verdict, so a breakdown would repeat it. */
  if (Actors.size() > 1) {
    for (size_t i = 0; i < Actors.size(); i++) {
      const Units::FBSimUnit &a = *Actors[i];
      FBLogUnitScope us(a.LogLabel());
      FBLog::Info("mission", "UNIT_RESULT", {{"result", ActorResultStr(a)}, {"reason", ActorReason(a)},
          {"team", FBUnitTeamStr(a.GetTeam())}, {"decisive", &a == deciding},
          {"lat", a.State().lat}, {"lon", a.State().lon}, {"altM", a.State().elev},
          {"telemetry", TelemetryPath(outDir, i, a)}});
    }
  }

  /* What the campaign layer takes away from this run — read off the SAME actors the verdict was read
   * off, so nothing is derived twice. Only the DECLARED blocks are asked; a released store became an
   * actor of its own and is nobody's cast. */
  if (carry && carry->Out) {
    for (size_t i = 0; i < mission.Units.size() && i < Actors.size(); i++) {
      Units::FBSimUnit &a = *Actors[i];
      Weapons::FBStoresSystem &sms = a.Module().Stores();
      int remaining[kFBStoreKinds], declared[kFBStoreKinds] = {};
      for (int k = 0; k < kFBStoreKinds; k++) remaining[k] = 0;
      for (const auto &kv : mission.Units[i].SetKV) {
        int station = 0, kind = -1;
        if (kv.first == "store" && FBParseStoreSetValue(kv.second, station, kind)) declared[kind]++;
      }
      for (int s = 1; s <= sms.StationCount(); s++) {
        const FBStoreSpec *spec = FBStoreSpecOf(sms.StoreAt(s));
        const int k = spec ? FBStoreKindIndex(spec->Key) : -1;
        if (k >= 0) remaining[k]++;
      }
      /* A kind this sortie never carried keeps whatever the book already said about it. */
      for (int k = 0; k < kFBStoreKinds; k++)
        if (declared[k] == 0) remaining[k] = -1;
        else carry->Out->ExpendedByKind[k] += declared[k] - remaining[k];
      carry->Out->State.Note(a.GetName(), a.GetKind() == Units::FBUnitKind::Ground,
                             !a.Health().CombatEffective(), remaining);
    }
  }

  double wallS = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
  FBLog::SetTime(simT);
  FBLogUnitScope us(deciding ? deciding->LogLabel() : std::string());
  FBLog::Info("mission", "RESULT", {{"result", FBMissionResultStr(result)}, {"reason", reason},
      {"lat", st.lat}, {"lon", st.lon}, {"altM", st.elev}, {"durationS", simT}});
  FBLog::Info("mission", "SUMMARY", {{"result", FBMissionResultStr(result)}, {"durationS", simT},
      {"wallS", wallS}, {"speedup", wallS > 0.0 ? simT / wallS : 0.0},
      {"lat", st.lat}, {"lon", st.lon}, {"altM", st.elev}});
  switch (result) {
    case FBMissionResult::Success: return 0;
    case FBMissionResult::Fail: return 1;
    case FBMissionResult::Crash: return 2;
    case FBMissionResult::Loc: return 2;   /* shares Crash's exit code — see FBMissionResult's banner */
    case FBMissionResult::Timeout: return 3;
  }
  return 1;
}

} // namespace FlightBox::Missions
