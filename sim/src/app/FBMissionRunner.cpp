#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBModuleRegistry.h"
#include "FBTelemetry.h"
#include "FBTelemetrySinks.h"
#include "FBFdmTelemetrySource.h"
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

namespace {
constexpr double kPi = 3.14159265358979323846;

/* "off the runway" test for the crash gate: project (lat,lon) onto the runway's along/across-track
 * axes (centreline from the threshold on TrueHeadingDeg) — on the runway iff within its length (+
 * marginAlongM before/after) and half-width (+ marginAcrossM either side). WidthM <= 1 (the mission
 * format leaves it unset) falls back to a generous 60 m generic-runway half-width. */
bool OnRunway(const FBRunway &rwy, double lat, double lon, double marginAlongM, double marginAcrossM) {
  double hdg = rwy.TrueHeadingDeg * kPi / 180.0;
  double coslat = std::cos(rwy.ThresholdLatDeg * kPi / 180.0);
  double dy = (lat - rwy.ThresholdLatDeg) * 111320.0;
  double dx = (lon - rwy.ThresholdLonDeg) * 111320.0 * coslat;
  double along = dx * std::sin(hdg) + dy * std::cos(hdg);
  double across = dx * std::cos(hdg) - dy * std::sin(hdg);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5 + marginAcrossM;
  return along >= -marginAlongM && along <= rwy.LengthM + marginAlongM && std::fabs(across) <= halfW;
}
} // namespace

const char *FBMissionResultStr(FBMissionResult r) {
  switch (r) {
    case FBMissionResult::Success: return "SUCCESS";
    case FBMissionResult::Fail: return "FAIL";
    case FBMissionResult::Crash: return "CRASH";
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
  const FBRunway &rwy = mission.Runway;
  FBLog::Info("mission", "MISSION_START", {{"name", mission.Name}, {"runway_hdg", rwy.TrueHeadingDeg},
                                           {"timeout", timeoutS}});

  double groundAsl = elevation.GroundElevM(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
  if (groundAsl <= -1e8) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "elevation unresolved at spawn"}});
    fclose(evf);
    return 1;
  }
  FBLog::Info("mission", "SPAWN", {{"name", mission.Name}, {"lat", rwy.ThresholdLatDeg},
                                   {"lon", rwy.ThresholdLonDeg}, {"groundM", groundAsl},
                                   {"fileElevM", rwy.ThresholdElevM}, {"hdg", rwy.TrueHeadingDeg},
                                   {"lenM", rwy.LengthM}});

  FBRegisterBuiltinModules();
  std::unique_ptr<FBModule> ModulePtr = FBModuleRegistry::Create(mission.ModuleName);
  if (!ModulePtr) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "unknown module '" + mission.ModuleName + "'"}});
    fclose(evf);
    return 1;
  }
  FBModule &Module = *ModulePtr;

  fb_fdm_state St{};
  if (!FBMissionGroundSpawn(aircraftPath.c_str(), mission, groundAsl, Module, St)) {
    FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "jsbsim init"}});
    fclose(evf);
    return 1;
  }

  if (hook) hook->OnMissionStart(rwy, groundAsl, Module.Displays());

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

  const double dt = 0.1;
  const double kWpCaptureM = 500.0;
  double simT = 0.0;
  FBMissionResult result = FBMissionResult::Timeout;
  std::string reason = "sim time exceeded the mission timeout";
  auto lastPhase = Module.PilotSystem().GetPhase();
  bool warnedStall = false, warnedOverspeed = false, warnedSink = false;
  clock_t wallStart = clock();

  while (simT < timeoutS) {
    double gnd = elevation.GroundElevM(St.lat, St.lon);
    if (gnd > -1e8) groundAsl = gnd;
    Module.SetGroundAsl((float)groundAsl);
    fb_jsbsim_set_ground(groundAsl);

    Module.Run(St, dt, nullptr);
    simT += dt;
    FBLog::SetTime(simT);

    auto phase = Module.PilotSystem().GetPhase();
    if (phase != lastPhase) {
      FBLog::Info("pilot", "PHASE", {{"from", FBPilot::PhaseName(lastPhase)}, {"to", FBPilot::PhaseName(phase)}});
      lastPhase = phase;
    }

    FBFlightPlan &plan = Module.FlightPlan();
    double distToWpM = -1.0;
    if (const FBWaypoint *wp = plan.ActiveWaypoint()) {
      double dy = (St.lat - wp->LatDeg) * 111320.0, dx = (St.lon - wp->LonDeg) * 111320.0 * std::cos(St.lat * kPi / 180.0);
      distToWpM = std::sqrt(dx * dx + dy * dy);
    }
    int reachedWp = FBMissionAdvanceWaypoint(plan, St.lat, St.lon, kWpCaptureM);
    if (reachedWp >= 0) {
      const FBWaypoint &wp = plan.At(reachedWp);
      FBLog::Info("pilot", "WP_REACHED", {{"idx", reachedWp}, {"lat", wp.LatDeg}, {"lon", wp.LonDeg}, {"distM", distToWpM}});
    }

    /* AoA is numerically undefined at near-zero airspeed — see FBAppNative.cpp's original banner for
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

    bool wow = fb_jsbsim_get_wow(-1) != 0;
    bool penetration = aglM < -3.0;
    bool offRunwayContact = wow && !OnRunway(rwy, St.lat, St.lon, 50.0, 30.0);
    if (penetration || offRunwayContact) {
      result = FBMissionResult::Crash;
      reason = penetration ? "ground penetration" : "hard contact off the runway";
      Module.Controls().EngineCutoff();
      FBLog::Warn("pilot", "WARN", {{"kind", "crash"}, {"detail", reason}, {"lat", St.lat}, {"lon", St.lon}, {"aglM", aglM}});
      break;
    }

    bus.Tick(simT);
    if (hook) hook->OnTick(St, simT, groundAsl, aglM, Module.Telemetry());

    if (phase == FBPilot::Phase::Shutdown) { result = FBMissionResult::Success; reason = "mission phases complete"; break; }
  }

  double wallS = (double)(clock() - wallStart) / CLOCKS_PER_SEC;
  FBLog::SetTime(simT);
  FBLog::Info("mission", "RESULT", {{"result", FBMissionResultStr(result)}, {"reason", reason},
      {"phase", FBPilot::PhaseName(Module.PilotSystem().GetPhase())}, {"lat", St.lat}, {"lon", St.lon},
      {"altM", St.elev}, {"durationS", simT}});
  FBLog::Info("mission", "SUMMARY", {{"result", FBMissionResultStr(result)}, {"durationS", simT},
      {"wallS", wallS}, {"speedup", wallS > 0.0 ? simT / wallS : 0.0},
      {"phase", FBPilot::PhaseName(Module.PilotSystem().GetPhase())}, {"lat", St.lat}, {"lon", St.lon}, {"altM", St.elev}});
  FBLog::SetSink(nullptr);
  fclose(evf);
  fclose(csv);

  switch (result) {
    case FBMissionResult::Success: return 0;
    case FBMissionResult::Fail: return 1;
    case FBMissionResult::Crash: return 2;
    case FBMissionResult::Timeout: return 3;
  }
  return 1;
}

} // namespace FlightBox
