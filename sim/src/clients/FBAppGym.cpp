/* fb-gym: the headless mission client — mission in, telemetry out. Links the core library and the
 * shared mission loop, and critically carries NO Dawn/WebGPU symbol at all (the Makefile target's own
 * nm check enforces it). --elev picks the ground-truth provider; the default is `swiss` when the baked
 * asset is on disk and `const` otherwise, so a bare `fb-gym --mission FILE` always runs, network or
 * not. doc/flightbox/build-and-ops.md. */
#include "FBMissionRunner.h"
#include "FBMissionFile.h"
#include "FBRunwayPlateauElevation.h"
#include "FBBakedDemElevation.h"
#include "FBTilesElevation.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using namespace FlightBox;

namespace {

const char *kDefaultSwissDem = "assets/swiss-dem-90m.bin";

void Usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s --mission FILE [--out DIR] [--timeout N] [--threads N] [--elev tiles|const|swiss] [--base URL]\n"
          "  --mission FILE   ground-spawn a .fbm mission (doc/mission-format.md) on its runway threshold\n"
          "                   and run headless (JSBSim + the module's FBPilot phase machine) until SUCCESS/CRASH/\n"
          "                   TIMEOUT/FAIL; writes --out/telemetry.csv + --out/events.log, exit 0/1/2/3.\n"
          "  --timeout N      overrides the mission file's own timeout (sim-seconds)\n"
          "  --threads N      threads that step the mission's units, the main thread included (default 1 =\n"
          "                   the sequential reference path; clamped to the unit count). Parallelises the\n"
          "                   per-unit STEP only — verdicts, telemetry and log order stay sequential, so a\n"
          "                   run's outputs are byte-identical whatever N is. GYM ONLY, by design.\n"
          "  --elev tiles|const|swiss  ground-elevation source (default: swiss if %s exists, else const):\n"
          "                   tiles  = live fb-tiles /elev (--base, default http://localhost:8081)\n"
          "                   const  = the mission's own runway(s) on a flat base (FBRunwayPlateauElevation,\n"
          "                            no data/network needed — 'flat')\n"
          "                   swiss  = the baked Switzerland DEM island (tools/bake_swiss_dem.py)\n"
          "  --swiss-dem PATH override the baked-DEM asset path (default %s)\n",
          argv0, kDefaultSwissDem, kDefaultSwissDem);
}

bool FileExists(const std::string &path) {
  std::ifstream f(path);
  return f.good();
}

} // namespace

int main(int argc, char **argv) {
  std::string missionPath, outDir = ".", base = "http://localhost:8081", elevMode, swissDemPath = kDefaultSwissDem;
  double timeout = 0.0;
  long threads = 1;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--threads" && i + 1 < argc) threads = atol(argv[++i]);
    else if (a == "--mission" && i + 1 < argc) missionPath = argv[++i];
    else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
    else if (a == "--timeout" && i + 1 < argc) timeout = atof(argv[++i]);
    else if (a == "--elev" && i + 1 < argc) elevMode = argv[++i];
    else if (a == "--base" && i + 1 < argc) base = argv[++i];
    else if (a == "--swiss-dem" && i + 1 < argc) swissDemPath = argv[++i];
    else { Usage(argv[0]); return 1; }
  }
  if (missionPath.empty()) { Usage(argv[0]); return 1; }
  if (threads < 1) { fprintf(stderr, "fb-gym: --threads must be >= 1\n"); return 1; }
  if (!FBEnsureDir(outDir)) { fprintf(stderr, "fb-gym: cannot create --out %s\n", outDir.c_str()); return 1; }

  static FBStdoutLogSink gStdoutSink;
  FBLog::SetSink(&gStdoutSink);
  FBLog::SetLevel(FBLogLevel::Debug);

  if (elevMode.empty()) elevMode = FileExists(swissDemPath) ? "swiss" : "const";

  /* This provider needs the mission's runways BEFORE FBRunMission parses the file, and the parser is
   * pure string-in/struct-out — so a second cheap parse beats threading a deferred provider through
   * the runner for one CLI mode. */
  std::unique_ptr<FBElevationProvider> elevation;
  if (elevMode == "tiles") {
    elevation = std::make_unique<FBTilesElevation>(base.c_str());
  } else if (elevMode == "swiss") {
    auto baked = std::make_unique<FBBakedDemElevation>(swissDemPath);
    if (!baked->Ok())
      FBLog::Warn("gym", "swiss_dem_load_failed", {{"path", swissDemPath}, {"fallback", "0 m everywhere in the asset's absence"}});
    elevation = std::move(baked);
  } else if (elevMode == "const") {
    std::ifstream in(missionPath);
    std::vector<FBRunway> runways;
    if (in) {
      std::stringstream buf; buf << in.rdbuf();
      FBMission mission; std::string perr;
      if (FBParseMissionFile(buf.str(), mission, &perr) && mission.HaveRunway) runways.push_back(mission.Runway);
    }
    elevation = std::make_unique<FBRunwayPlateauElevation>(std::move(runways));
  } else {
    fprintf(stderr, "fb-gym: unknown --elev '%s' (want tiles|const|swiss)\n", elevMode.c_str());
    return 1;
  }
  FBLog::Info("gym", "elevation_provider", {{"mode", elevMode}});
  /* Announced HERE and never inside the run: a thread-count line in events.log would be the single
   * byte of difference between a sequential and a parallel run. */
  FBLog::Info("gym", "step_threads", {{"requested", (int)threads}});

  return FBRunMission(missionPath, timeout, outDir, FBNativeModelRoots(), *elevation, nullptr,
                      (size_t)threads);
}
