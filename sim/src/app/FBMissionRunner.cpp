#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBModuleRegistry.h"
#include "FBTelemetry.h"
#include "FBTelemetrySinks.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cerrno>
#include <cstdio>
#include <ctime>
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

using FBActorList = std::vector<std::unique_ptr<FBSimUnit>>;

/* A physical K.O. of ANY actor ends the run — today's ownship-is-the-run rule, generalised, and
 * deliberately the conservative reading: with several actors one departing airframe still stops the
 * loop rather than leaving a wreck integrating in the background. WHOSE K.O. it was decides the RESULT
 * line, which is why this returns the unit and not a bool. */
const FBSimUnit *FirstFlightKo(const FBActorList &actors) {
  for (const auto &a : actors)
    if (a->FlightMonitor().Tripped()) return a.get();
  return nullptr;
}

/* telemetry.csv per actor: the PRIMARY keeps the canonical name (every existing tool and every
 * regression hash reads outDir/telemetry.csv), each further actor gets its own file with the same fixed
 * schema. One file per unit rather than one wide row: an actor's column set follows ITS module, so a
 * shared row would either force every module into one schema or make the header depend on the mission's
 * cast — and a per-unit file needs no special case at N=1. */
std::string TelemetryPath(const std::string &outDir, size_t index, const FBSimUnit &unit) {
  if (index == 0) return outDir + "/telemetry.csv";
  char suffix[32];
  snprintf(suffix, sizeof suffix, "/telemetry_u%d.csv", unit.GetId());
  return outDir + suffix;
}

/* The file + sink behind one actor's telemetry bus. app/ owns the I/O (core/ stays I/O-free), the unit
 * owns the bus — this pairs them for the run's lifetime, one entry per actor. */
struct FBActorTelemetry {
  FBFileHandle File{nullptr, &fclose};
  std::unique_ptr<FBCsvTelemetrySink> Sink;
};
} // namespace

int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const std::string &aircraftPath, FBElevationProvider &elevation,
                 FBMissionTickHook *hook) {
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
   * One block per actor the mission declares; today's .fbm declares exactly one (see the header
   * banner). Everything an actor needs — elevation-resolved spawn, airframe, module, both monitors,
   * telemetry — is produced here and owned by the list from then on. */
  FBRegisterBuiltinModules();
  FBActorList Actors;
  {
    const FBSpawn &sp = mission.Spawn;
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
    FBLog::Info("mission", "SPAWN", {{"name", mission.Name}, {"lat", sp.LatDeg}, {"lon", sp.LonDeg},
        {"ground", sp.Ground}, {"altM", sp.Ground ? groundAsl : sp.AltM}, {"groundAsl", groundAsl},
        {"hdg", sp.HeadingDeg}, {"speedKt", sp.SpeedKt}});

    std::string serr;
    std::unique_ptr<FBSimUnit> unit = FBMissionSpawnActor(aircraftPath.c_str(), mission, groundAsl,
                                                          timeoutS, (int)Actors.size() + 1, &serr);
    if (!unit) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", serr}});
      return 1;
    }
    Actors.push_back(std::move(unit));
  }

  if (hook) hook->OnMissionStart(mission.Spawn, *Actors.front());

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
  clock_t wallStart = clock();

  /* The MISSION verdict is the primary actor's (the .fbm's plan/runway describe that one); the PHYSICAL
   * K.O. is anyone's. Etappe 3, where every unit carries its own objectives, extends the first half —
   * the loop shape stays as it is. */
  while (!FirstFlightKo(Actors) && !Actors.front()->MissionConcluded()) {
    for (auto &a : Actors) a->UpdateGroundAsl(elevation.GroundElevM(a->State().lat, a->State().lon));
    for (auto &a : Actors) a->Run(dt, nullptr);
    simT += dt;
    FBLog::SetTime(simT);
    for (auto &a : Actors) {
      a->CheckEnvelope();   /* generic envelope diagnostics — per actor, not per run */
      a->RunMonitors(simT);
    }
    for (auto &a : Actors) a->SampleTelemetry(simT);
    if (hook) hook->OnTick(*Actors.front(), simT);
  }

  /* ---- Step 4: validate the world — the monitors already did; translate their verdict ---- */
  const FBSimUnit &primary = *Actors.front();
  const FBSimUnit *ko = FirstFlightKo(Actors);
  FBMissionResult result = ko
      ? (ko->FlightMonitor().Reason() == FBKoReason::Loc ? FBMissionResult::Loc : FBMissionResult::Crash)
      : ToMissionResult(primary.MissionMonitor()->Verdict());
  std::string reason = ko ? ko->FlightMonitor().Detail() : primary.MissionMonitor()->Detail();
  const fb_fdm_state &st = primary.State();

  double wallS = (double)(clock() - wallStart) / CLOCKS_PER_SEC;
  FBLog::SetTime(simT);
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
