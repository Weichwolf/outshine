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

/* The tick's STEP phase as one job (missions/FBTickPool.h): index i steps actor i and nothing else —
 * WHAT stepping is belongs to FBMissionSim, which hands the step in. The sim time is stamped inside
 * RunIndex because FBLog's clock is thread-local, and the per-actor sink is what makes a threaded run's
 * log order the actor order. */
class FBActorStepJob : public FBTickJob {
public:
  FBActorStepJob(const FBActorStep &step, std::vector<Clients::FBBufferedLogSink> &logs, double simT)
      : Step_(step), Logs_(logs), TimeS_(simT) {}

  void RunIndex(size_t i) override {
    FBLog::SetTime(TimeS_);
    FBLogThreadSinkScope capture(&Logs_[i]);
    Step_(i);
  }

private:
  const FBActorStep &Step_;
  std::vector<Clients::FBBufferedLogSink> &Logs_;
  double TimeS_;
};

/* THE ONE PHASE OF A TICK THIS CLIENT REPLACES (missions/FBMissionSim.h): the cast is stepped across a
 * pool, and the per-actor log buffers are replayed in ACTOR ORDER at the barrier — which is why a run's
 * events.log is byte-identical whatever the thread count. Nothing about the pool is LOGGED: how many
 * threads stepped the cast is a property of the client, and a line about it would be the one difference
 * between a sequential and a parallel run. */
class FBPoolStepper : public FBActorStepper {
public:
  FBPoolStepper(size_t threads, size_t maxActors, FBLogSink &sink)
      : Logs_(maxActors), Sink_(sink), Pool_(threads) {}

  void Step(const FBActorStep &step, size_t count, double simT) override {
    FBActorStepJob job(step, Logs_, simT);
    Pool_.RunTick(job, count);
    for (auto &l : Logs_) l.Drain(Sink_);
  }

private:
  std::vector<Clients::FBBufferedLogSink> Logs_;
  FBLogSink &Sink_;
  FBTickPool Pool_;   /* declared LAST, so it is destroyed FIRST: its threads are joined while the
                       * buffers and the job they were handed are still alive */
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

  /* Only an `identify` objective reads a range, so the ranges the loop computes are only computed for a
   * mission that declares one — read off the mission here and handed to the simulation below. */
  bool NeedRanges = false;
  for (const auto &u : mission.Units)
    for (const auto &o : u.Objectives) NeedRanges = NeedRanges || o.Kind == FBObjectiveKind::Identify;

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

  /* ---- Step 3: execute the actors ----
   * THE LOOP IS NOT WRITTEN HERE. It is missions/FBMissionSim, the one object that owns a tick and the
   * rule that ends a run; this client hands it what a headless run has that a browser has not — a
   * thread pool for the STEP phase — and drives it to conclusion. */
  /* steady_clock, not clock(): the latter sums every thread's CPU time and would report a FASTER
   * parallel run as a slower one. */
  auto wallStart = std::chrono::steady_clock::now();

  if (threads < 1) threads = 1;
  if (threads > Actors.size()) threads = Actors.size();
  FBPoolStepper stepper(threads, maxActors, logSink);

  FBMissionSim sim(Actors, UnitReg, Ordnance, elevation, *weather, timeoutS);
  sim.SetClock(clock);
  sim.SetRangeAware(NeedRanges);
  sim.SetHook(hook);
  sim.SetStepper(&stepper);
  const FBMissionResult result = sim.RunToConclusion();

  /* ---- Step 4: report what the judges concluded ---- */
  const Units::FBSimUnit &primary = *Actors.front();
  const Units::FBSimUnit *deciding = sim.Deciding();
  const double simT = sim.SimTimeS();
  const std::string &reason = sim.Reason();
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
  return sim.ExitCode();   /* the process contract, spelled once (missions/FBMissionSim.cpp) */
}

} // namespace FlightBox::Missions
