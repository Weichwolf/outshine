#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBModuleRegistry.h"
#include "FBTelemetry.h"
#include "FBTelemetrySinks.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBTickPool.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace FlightBox {

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
/* FBMissionVerdict + FBFlightMonitor's FBKoReason -> the ONE FBMissionResult this Runner returns —
 * both monitors run independently (see the file banner); this is the one place their two verdicts
 * combine into a single exit code, not a third judgement of its own. */
FBMissionResult ToMissionResult(FBMissionVerdict v) {
  switch (v) {
    case FBMissionVerdict::Success: return FBMissionResult::Success;
    case FBMissionVerdict::Fail: return FBMissionResult::Fail;
    case FBMissionVerdict::Timeout: return FBMissionResult::Timeout;
    case FBMissionVerdict::None: break;
  }
  return FBMissionResult::Timeout;   /* unreachable in practice: only called once Concluded() */
}

/* A physical K.O. of ANY actor ends the run — today's ownship-is-the-run rule, generalised, and
 * deliberately the conservative reading: with several actors one departing airframe still stops the
 * loop rather than leaving a wreck integrating in the background. WHOSE K.O. it was decides the RESULT
 * line, which is why this returns the unit and not a bool. */
const FBSimUnit *FirstFlightKo(const FBActorList &actors) {
  for (const auto &a : actors)
    if (a->FlightMonitor().Tripped()) return a.get();
  return nullptr;
}

/* The MISSION verdict is per actor and combined here, nowhere else: an actor is JUDGED iff the mission
 * gave it objectives (it then carries an FBMissionMonitor, app/FBMissionBoot.h). The run is over the
 * moment ONE judged actor fails or times out (there is nothing left to prove), and it succeeds only
 * once EVERY judged actor has reached its own verdict of Success. */
const FBSimUnit *FirstMissionFailure(const FBActorList &actors) {
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (m && m->Concluded() && m->Verdict() != FBMissionVerdict::Success) return a.get();
  }
  return nullptr;
}
bool AllObjectivesMet(const FBActorList &actors) {
  bool anyJudged = false;
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (!m) continue;
    anyJudged = true;
    if (m->Verdict() != FBMissionVerdict::Success) return false;
  }
  return anyJudged;
}
/* The judged actor whose verdict/detail the combined RESULT quotes on SUCCESS — the first one, matching
 * the single-actor case exactly (there the primary IS the only judged actor). */
const FBSimUnit *FirstJudged(const FBActorList &actors) {
  for (const auto &a : actors)
    if (a->MissionMonitor()) return a.get();
  return nullptr;
}

/* telemetry.csv per actor: the PRIMARY keeps the canonical name (every existing tool and every
 * regression hash reads outDir/telemetry.csv), each further actor gets a file named after its callsign
 * (validated filename-safe by the parser) with the same fixed schema. One file per unit rather than one
 * wide row: an actor's column set follows ITS module, so a shared row would either force every module
 * into one schema or make the header depend on the mission's cast — and a per-unit file needs no
 * special case at N=1. */
std::string TelemetryPath(const std::string &outDir, size_t index, const FBSimUnit &unit) {
  if (index == 0) return outDir + "/telemetry.csv";
  return outDir + "/telemetry_" + unit.GetName() + ".csv";
}

/* This actor's own result string for the per-unit breakdown: the physical judge outranks the mission
 * judge (a wreck has no mission verdict worth quoting), an actor without objectives reports NONE. */
const char *ActorResultStr(const FBSimUnit &a) {
  if (a.FlightMonitor().Tripped())
    return a.FlightMonitor().Reason() == FBKoReason::Loc ? "LOC" : "CRASH";
  const FBMissionMonitor *m = a.MissionMonitor();
  return m ? FBMissionVerdictStr(m->Verdict()) : "NONE";
}
std::string ActorReason(const FBSimUnit &a) {
  if (a.FlightMonitor().Tripped()) return a.FlightMonitor().Detail();
  const FBMissionMonitor *m = a.MissionMonitor();
  if (!m) return "no objectives";
  return m->Concluded() ? m->Detail() : "still under way when the run ended";
}

/* The file + sink behind one actor's telemetry bus. app/ owns the I/O (core/ stays I/O-free), the unit
 * owns the bus — this pairs them for the run's lifetime, one entry per actor. */
struct FBActorTelemetry {
  FBFileHandle File{nullptr, &fclose};
  std::unique_ptr<FBCsvTelemetrySink> Sink;
};

/* The tick's STEP phase as one job (app/FBTickPool.h): index i is actor i stepping its own airframe and
 * module for `dt`, nothing else. Everything the step could otherwise share is kept out of it — the world
 * pointer is null here exactly as it always was, so no actor reaches the unit registry, and log output
 * goes into the actor's OWN buffer rather than the run's sink. The sim time is stamped inside RunIndex
 * because FBLog's clock is thread-local (core/FBLog.h): each worker learns the tick it is in from the
 * job, which is also the only per-tick state this object carries. */
class FBActorStepJob : public FBTickJob {
public:
  FBActorStepJob(FBActorList &actors, std::vector<FBBufferedLogSink> &logs, double dt)
      : Actors_(actors), Logs_(logs), Dt_(dt) {}

  void SetTime(double simT) { TimeS_ = simT; }

  void RunIndex(size_t i) override {
    FBLog::SetTime(TimeS_);
    FBLogThreadSinkScope capture(&Logs_[i]);
    FBLogUnitScope us(Actors_[i]->LogLabel());
    Actors_[i]->Run(Dt_, nullptr);
  }

private:
  FBActorList &Actors_;
  std::vector<FBBufferedLogSink> &Logs_;
  double Dt_;
  double TimeS_ = 0.0;
};
} // namespace

int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const std::string &aircraftPath, FBElevationProvider &elevation,
                 FBMissionTickHook *hook, size_t threads) {
  std::string evPath = outDir + "/events.log";
  FBFileHandle evf = FBOpenFile(evPath.c_str(), "w");
  if (!evf) { fprintf(stderr, "mission: cannot open %s for writing\n", evPath.c_str()); return 1; }

  /* Declaration order IS the cleanup contract (FBLogSinkScope's banner): the scope is declared last, so
   * it is destroyed first and FBLog's sink pointer is cleared before the sinks and the FILE* behind them
   * go away — on EVERY return below, not just the successful one. A second mission in the same process
   * (the planned pilot tournaments) would otherwise log through a dangling, already-closed sink. */
  FBFileLogSink fileSink(evf.get());
  FBStdoutLogSink stdoutSink;
  FBCompositeLogSink logSink;
  logSink.Add(&fileSink);
  logSink.Add(&stdoutSink);
  FBLogSinkScope logScope(&logSink);
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

  /* ---- Step 2: set up the world with its actors ----
   * One block per actor the mission declares (core/FBMissionFile.h). Everything an actor needs —
   * elevation-resolved spawn, airframe, module, its own monitors, telemetry — is produced here and
   * owned by the list from then on; the list's ORDER is the mission file's order and stays the tick
   * order for the whole run. */
  FBRegisterBuiltinModules();
  FBActorList Actors;
  Actors.reserve(mission.Units.size());
  for (size_t i = 0; i < mission.Units.size(); i++) {
    const FBMissionUnit &block = mission.Units[i];
    const FBSpawn &sp = block.Spawn;
    /* Attribution for everything this actor's spawn emits — empty label for a single-actor mission
     * (core/FBLog.h). The unit itself does not exist yet, so the rule is read from the mission here and
     * from the unit (FBSimUnit::LogLabel) in every loop below. */
    FBLogUnitScope us(mission.Units.size() > 1 ? block.Id : std::string());
    double groundAsl = elevation.GroundElevM(sp.LatDeg, sp.LonDeg);
    if (!FBElevationResolved(groundAsl)) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "elevation unresolved at spawn"}});
      return 1;
    }
    /* Consistency validation (declarative-spawn contract, doc/mission-format.md): an explicit altitude
     * placed below the resolved terrain is a genuine contradiction, not a legal (if unusual)
     * declaration — a 1 m margin absorbs elevation-source rounding, not real penetration. */
    if (!sp.Ground && sp.AltM < groundAsl - 1.0) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "spawn altitude is below ground"},
          {"altM", sp.AltM}, {"groundM", groundAsl}});
      return 1;
    }
    /* WHICH actor this spawn is comes from the scope's `unit=` attribution above, not from a second
     * name field — `name` stays the MISSION's, exactly as it always read. */
    FBLog::Info("mission", "SPAWN", {{"name", mission.Name}, {"lat", sp.LatDeg}, {"lon", sp.LonDeg},
        {"ground", sp.Ground}, {"altM", sp.Ground ? groundAsl : sp.AltM}, {"groundAsl", groundAsl},
        {"hdg", sp.HeadingDeg}, {"speedKt", sp.SpeedKt}});

    std::string serr;
    std::unique_ptr<FBSimUnit> unit = FBMissionSpawnActor(aircraftPath.c_str(), mission, i, groundAsl,
                                                          timeoutS, &serr);
    if (!unit) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", serr}});
      return 1;
    }
    Actors.push_back(std::move(unit));
  }

  if (hook) hook->OnMissionStart(mission.Units.front().Spawn, Actors);

  std::vector<FBActorTelemetry> ActorTelemetry(Actors.size());
  for (size_t i = 0; i < Actors.size(); i++) {
    std::string path = TelemetryPath(outDir, i, *Actors[i]);
    ActorTelemetry[i].File = FBOpenFile(path.c_str(), "w");
    if (!ActorTelemetry[i].File) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "cannot open telemetry.csv"}});
      return 1;
    }
    ActorTelemetry[i].Sink = std::make_unique<FBCsvTelemetrySink>(ActorTelemetry[i].File.get());
    Actors[i]->StartTelemetry(ActorTelemetry[i].Sink.get());
  }

  /* ---- Step 3: execute the actors ---- */
  const double dt = 0.1;
  double simT = 0.0;
  /* steady_clock, not clock(): with a worker thread per actor clock() reports the SUM of every thread's
   * CPU time, so the SUMMARY's `wallS`/`speedup` would get worse the faster the run actually got. */
  auto wallStart = std::chrono::steady_clock::now();

  /* The STEP phase's execution resources. The pool is sized here rather than taken as the caller's raw
   * wish: threads beyond the cast size would only park on the barrier. One capture buffer per actor,
   * alive for the whole run so a steady-state tick allocates nothing. Nothing about the pool is LOGGED —
   * how many threads stepped the cast is a property of the client, not an event of the mission, and a
   * line about it would be the one difference between a sequential and a parallel events.log.
   * The pool is declared LAST for the same reason FBLogSinkScope is (see its banner): reverse
   * declaration order means it is destroyed FIRST, so its threads are joined while the buffers and the
   * job they were handed are still alive. */
  if (threads < 1) threads = 1;
  if (threads > Actors.size()) threads = Actors.size();
  std::vector<FBBufferedLogSink> actorLogs(Actors.size());
  FBActorStepJob stepJob(Actors, actorLogs, dt);
  FBTickPool pool(threads);

  /* The run ends on the first physical K.O. of ANY actor, the first mission FAILURE of any judged
   * actor, or once EVERY judged actor has met its own objectives (see the helpers above). The trailing
   * timeout guard is the backstop for a cast with no objectives at all — every judged actor's own
   * FBMissionMonitor concludes TIMEOUT at exactly this sim time, so it never preempts one.
   *
   * SNAPSHOT DISCIPLINE (FBUnit::GetPose's contract): the per-actor passes are separate loops on
   * purpose — every actor integrates against the poses of the LAST completed tick, and only the barrier
   * after all of them publishes the new ones. No actor can therefore see a neighbour that has already
   * stepped this tick, so tick ORDER cannot influence the result — which is what lets the STEP pass run
   * one thread per actor (app/FBTickPool.h) while EVERY other pass stays a plain sequential loop in
   * actor order:
   *   - elevation sampling, because the provider is the client's one shared object (FBTilesElevation
   *     drives the tile streamer) and a per-tick point query is far too cheap to be worth the question;
   *   - pose publication, which IS the barrier;
   *   - monitors + envelope checks, so the verdict that ends a run and the lines it emits are read in
   *     actor order, never in finishing order;
   *   - telemetry sampling and the tick hook (the native oracle's renderer), single-threaded by decision.
   * The step pass's own log output is captured per actor and replayed here, in the same actor order the
   * sequential loop wrote it in. */
  while (!FirstFlightKo(Actors) && !FirstMissionFailure(Actors) && !AllObjectivesMet(Actors) &&
         simT < timeoutS) {
    for (auto &a : Actors) a->UpdateGroundAsl(elevation.GroundElevM(a->State().lat, a->State().lon));
    stepJob.SetTime(simT);
    pool.RunTick(stepJob, Actors.size());
    for (auto &l : actorLogs) l.Drain(logSink);
    for (auto &a : Actors) a->PublishPose();   /* the barrier: new poses become visible together */
    simT += dt;
    FBLog::SetTime(simT);
    for (auto &a : Actors) {
      FBLogUnitScope us(a->LogLabel());
      a->CheckEnvelope();   /* generic envelope diagnostics — per actor, not per run */
      a->RunMonitors(simT);
    }
    for (auto &a : Actors) a->SampleTelemetry(simT);
    if (hook) hook->OnTick(Actors, simT);
  }

  /* ---- Step 4: validate the world — the monitors already did; combine their verdicts ---- */
  const FBSimUnit &primary = *Actors.front();
  const FBSimUnit *ko = FirstFlightKo(Actors);
  const FBSimUnit *failed = ko ? nullptr : FirstMissionFailure(Actors);
  /* The actor whose verdict ENDED the run — a K.O. or the first mission failure. On a clean success
   * nobody decided anything alone (every judged actor met its own objectives), so this stays null and
   * the combined RESULT carries no unit attribution. */
  const FBSimUnit *deciding = ko ? ko : failed;
  const FBSimUnit *judged = FirstJudged(Actors);
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
  const fb_fdm_state &st = primary.State();

  /* Per-actor breakdown before the combined verdict: one machine-readable line per actor with its own
   * result, its own reason, where it ended up and which telemetry file holds its trace. Emitted only
   * for a real flight — with a single actor the RESULT line below IS that actor's verdict and a
   * breakdown would just repeat it (the same rule that leaves single-actor logs unattributed). */
  if (Actors.size() > 1) {
    for (size_t i = 0; i < Actors.size(); i++) {
      const FBSimUnit &a = *Actors[i];
      FBLogUnitScope us(a.LogLabel());
      FBLog::Info("mission", "UNIT_RESULT", {{"result", ActorResultStr(a)}, {"reason", ActorReason(a)},
          {"team", FBUnitTeamStr(a.GetTeam())}, {"decisive", &a == deciding},
          {"lat", a.State().lat}, {"lon", a.State().lon}, {"altM", a.State().elev},
          {"telemetry", TelemetryPath(outDir, i, a)}});
    }
  }

  double wallS = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
  FBLog::SetTime(simT);
  /* The combined verdict, attributed to the actor that decided it — with one actor the label is empty
   * and the line reads exactly as it always did. */
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

} // namespace FlightBox
