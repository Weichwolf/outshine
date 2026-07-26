#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBModuleRegistry.h"
#include "FBTelemetry.h"
#include "FBTelemetrySinks.h"
#include "FBFdmTelemetrySource.h"
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>
#include <sys/stat.h>

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
constexpr double kMsToKt = 1.9438444924406;

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
} // namespace

int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const std::string &aircraftPath, FBElevationProvider &elevation,
                 FBMissionTickHook *hook) {
  std::string csvPath = outDir + "/telemetry.csv", evPath = outDir + "/events.log";
  FILE *evf = fopen(evPath.c_str(), "w");
  if (!evf) { fprintf(stderr, "mission: cannot open %s for writing\n", evPath.c_str()); return 1; }

  FBFileLogSink fileSink(evf);
  FBStdoutLogSink stdoutSink;
  FBCompositeLogSink logSink;
  logSink.Add(&fileSink);
  logSink.Add(&stdoutSink);
  FBLog::SetSink(&logSink);
  FBLog::SetTime(0.0);

  /* ---- Step 1: load the mission ---- */
  std::ifstream in(missionPath);
  if (!in) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "cannot open " + missionPath}});
    fclose(evf);
    return 1;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  FBMission mission;
  std::string perr;
  if (!FBParseMissionFile(buf.str(), mission, &perr)) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "parse: " + perr}});
    fclose(evf);
    return 1;
  }
  double timeoutS = timeoutOverride > 0.0 ? timeoutOverride : mission.TimeoutS;
  FBLog::Info("mission", "MISSION_START", {{"name", mission.Name}, {"timeout", timeoutS}});

  /* ---- Step 2: set up the world with its actors ---- */
  const FBSpawn &sp = mission.Spawn;
  double groundAsl = elevation.GroundElevM(sp.LatDeg, sp.LonDeg);
  if (groundAsl <= -1e8) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "elevation unresolved at spawn"}});
    fclose(evf);
    return 1;
  }
  /* Consistency validation (declarative-spawn contract, doc/mission-format.md): an explicit altitude
   * placed below the resolved terrain is a genuine contradiction, not a legal (if unusual) declaration —
   * a 1 m margin absorbs elevation-source rounding, not real penetration. */
  if (!sp.Ground && sp.AltM < groundAsl - 1.0) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "spawn altitude is below ground"},
        {"altM", sp.AltM}, {"groundM", groundAsl}});
    fclose(evf);
    return 1;
  }
  FBLog::Info("mission", "SPAWN", {{"name", mission.Name}, {"lat", sp.LatDeg}, {"lon", sp.LonDeg},
      {"ground", sp.Ground}, {"altM", sp.Ground ? groundAsl : sp.AltM}, {"groundAsl", groundAsl},
      {"hdg", sp.HeadingDeg}, {"speedKt", sp.SpeedKt}});

  FBRegisterBuiltinModules();
  std::unique_ptr<FBModule> ModulePtr = FBModuleRegistry::Create(mission.ModuleName);
  if (!ModulePtr) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "unknown module '" + mission.ModuleName + "'"}});
    fclose(evf);
    return 1;
  }
  FBModule &Module = *ModulePtr;

  fb_fdm_state St{};
  if (!FBMissionApplySpawn(aircraftPath.c_str(), mission, groundAsl, Module, St)) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "spawn failed (jsbsim init or an unknown 'set' key)"}});
    fclose(evf);
    return 1;
  }

  if (hook) hook->OnMissionStart(mission.Spawn, groundAsl, Module.Displays());

  FILE *csv = fopen(csvPath.c_str(), "w");
  if (!csv) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "cannot open telemetry.csv"}});
    fclose(evf);
    return 1;
  }

  FBFdmTelemetrySource fdmSrc(St, groundAsl);
  FBCsvTelemetrySink csvSink(csv);
  FBTelemetryBus bus;
  bus.Register(&fdmSrc);
  bus.Register(&Module.AirDataSystem());
  bus.Register(&Module.PilotSystem());
  bus.Register(&Module.FlightControl());
  bus.Register(&Module.Controls());
  bus.SetSink(&csvSink);
  bus.Start();

  /* The two incorruptible judges (CLAUDE.md "Kein Cheaten"), fed read-only every tick below, never
   * given to Module/Pilot: FBFlightMonitor asks "did the airframe survive" (physics only, no mission
   * knowledge); FBMissionMonitor asks "did the MISSION succeed" (its own copy of the mission's plan/
   * runway/timeout, never the module's live state — see its own banner). Two instances, two questions,
   * never conflated. */
  FBFlightMonitor flightMonitor;
  FBMissionMonitor missionMonitor(mission.Plan, mission.Runway, mission.HaveRunway, timeoutS);

  /* ---- Step 3: execute the actors ---- */
  const double dt = 0.1;
  double simT = 0.0;
  bool warnedStall = false, warnedOverspeed = false, warnedSink = false;
  clock_t wallStart = clock();

  while (!flightMonitor.Tripped() && !missionMonitor.Concluded()) {
    double gnd = elevation.GroundElevM(St.lat, St.lon);
    if (gnd > -1e8) groundAsl = gnd;
    Module.SetGroundAsl((float)groundAsl);
    fb_jsbsim_set_ground(groundAsl);

    Module.Run(St, dt, nullptr);
    simT += dt;
    FBLog::SetTime(simT);

    /* Generic flight-envelope diagnostics (not mission specifics — every module has these quantities):
     * AoA is numerically undefined at near-zero airspeed, see the historical FBAppNative.cpp banner for
     * the measured settle-transient this gate avoids. */
    bool haveAirspeed = St.cas > 15.0;
    if (haveAirspeed && St.alphaDeg > 25.0 && !warnedStall) {
      FBLog::Warn("pilot", "WARN", {{"kind", "stall"}, {"aoaDeg", St.alphaDeg}});
      warnedStall = true;
    } else if (!haveAirspeed || St.alphaDeg < 20.0) warnedStall = false;
    if (St.mach > 1.2 && !warnedOverspeed) {
      FBLog::Warn("pilot", "WARN", {{"kind", "overspeed"}, {"mach", St.mach}});
      warnedOverspeed = true;
    } else if (St.mach < 1.1) warnedOverspeed = false;
    double aglM = St.elev - groundAsl;
    if (aglM < 150.0 && St.vy < -15.0 && !warnedSink) {
      FBLog::Warn("pilot", "WARN", {{"kind", "sink"}, {"vsMs", St.vy}, {"aglM", aglM}});
      warnedSink = true;
    } else if (St.vy > -5.0) warnedSink = false;

    if (flightMonitor.Tick(FBBuildFlightMonitorSample(St, groundAsl), simT))
      Module.Controls().EngineCutoff();   /* Crash/LOC -> Motor aus, JSBSim's own ground reactions do the rest — no freeze */
    else if (missionMonitor.Tick({St.lat, St.lon, fb_jsbsim_get_wow(-1) != 0, St.gs * kMsToKt}, simT) &&
            missionMonitor.Verdict() == FBMissionVerdict::Fail)
      Module.Controls().EngineCutoff();   /* touched down off the assigned runway */

    bus.Tick(simT);
    if (hook) hook->OnTick(St, simT, groundAsl, aglM, Module.Telemetry());
  }

  /* ---- Step 4: validate the world — the monitors already did; translate their verdict ---- */
  FBMissionResult result = flightMonitor.Tripped()
      ? (flightMonitor.Reason() == FBKoReason::Loc ? FBMissionResult::Loc : FBMissionResult::Crash)
      : ToMissionResult(missionMonitor.Verdict());
  std::string reason = flightMonitor.Tripped() ? flightMonitor.Detail() : missionMonitor.Detail();

  double wallS = (double)(clock() - wallStart) / CLOCKS_PER_SEC;
  FBLog::SetTime(simT);
  FBLog::Info("mission", "RESULT", {{"result", FBMissionResultStr(result)}, {"reason", reason},
      {"lat", St.lat}, {"lon", St.lon}, {"altM", St.elev}, {"durationS", simT}});
  FBLog::Info("mission", "SUMMARY", {{"result", FBMissionResultStr(result)}, {"durationS", simT},
      {"wallS", wallS}, {"speedup", wallS > 0.0 ? simT / wallS : 0.0},
      {"lat", St.lat}, {"lon", St.lon}, {"altM", St.elev}});
  FBLog::SetSink(nullptr);
  fclose(evf);
  fclose(csv);

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
