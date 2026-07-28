/* fb-gym: the headless mission client — mission in, telemetry out. Links the core library and the
 * shared mission loop, and critically carries NO Dawn/WebGPU symbol at all (the Makefile target's own
 * nm check enforces it). --elev picks the ground-truth provider; the default is `swiss` when the baked
 * asset is on disk and `const` otherwise, so a bare `fb-gym --mission FILE` always runs, network or
 * not. doc/build-and-ops.md. */
#include "FBMissionRunner.h"
#include "FBCampaignRunner.h"
#include "FBCampaignFile.h"
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
          "usage: %s --mission FILE | --campaign FILE [--out DIR] [--timeout N] [--threads N]\n"
          "          [--state FILE] [--carry LIST] [--elev tiles|const|swiss] [--base URL]\n"
          "  --campaign FILE  run a .fbc campaign (doc/missions/campaign.md): its missions in file order\n"
          "                   into --out/NN-<mission>/, carrying destroyed units, destroyed ground targets\n"
          "                   and expended stores from each into the next. Exit = the worst mission's.\n"
          "  --state FILE     run ONE mission with a campaign state file applied — the standalone replay\n"
          "                   of a campaign step (--mission only; the campaign writes its own)\n"
          "  --carry LIST     which facts --state applies, comma-separated units,ground,stores (default all)\n"
          "  --mission FILE   ground-spawn a .fbm mission (doc/missions/syntax.md) on its runway threshold\n"
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

/* Every .fbm this invocation will touch: the one mission, or the campaign's list in file order. A
 * campaign that does not parse yields nothing here and fails properly inside the campaign runner. */
std::vector<std::string> CampaignMissionPaths(const std::string &campaignPath,
                                              const std::string &missionPath) {
  if (campaignPath.empty()) return {missionPath};
  std::ifstream in(campaignPath);
  if (!in) return {};
  std::stringstream buf; buf << in.rdbuf();
  FBCampaign campaign;
  if (!FBParseCampaignFile(buf.str(), campaign, nullptr)) return {};
  size_t slash = campaignPath.find_last_of('/');
  const std::string base = slash == std::string::npos ? std::string(".") : campaignPath.substr(0, slash);
  std::vector<std::string> out;
  for (const std::string &rel : campaign.Missions)
    out.push_back(rel.empty() || rel[0] == '/' ? rel : base + "/" + rel);
  return out;
}

bool ParseCarryList(const std::string &list, uint8_t &out) {
  if (list.empty()) { out = kFBCarryAll; return true; }
  out = 0;
  size_t i = 0;
  while (i <= list.size()) {
    size_t comma = list.find(',', i);
    const std::string tok = list.substr(i, comma == std::string::npos ? std::string::npos : comma - i);
    if (tok == "units") out |= FBCarryUnits;
    else if (tok == "ground") out |= FBCarryGround;
    else if (tok == "stores") out |= FBCarryStores;
    else return false;
    if (comma == std::string::npos) break;
    i = comma + 1;
  }
  return out != 0;
}

} // namespace

int main(int argc, char **argv) {
  std::string missionPath, campaignPath, statePath, carryList;
  std::string outDir = ".", base = "http://localhost:8081", elevMode, swissDemPath = kDefaultSwissDem;
  double timeout = 0.0;
  long threads = 1;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--threads" && i + 1 < argc) threads = atol(argv[++i]);
    else if (a == "--mission" && i + 1 < argc) missionPath = argv[++i];
    else if (a == "--campaign" && i + 1 < argc) campaignPath = argv[++i];
    else if (a == "--state" && i + 1 < argc) statePath = argv[++i];
    else if (a == "--carry" && i + 1 < argc) carryList = argv[++i];
    else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
    else if (a == "--timeout" && i + 1 < argc) timeout = atof(argv[++i]);
    else if (a == "--elev" && i + 1 < argc) elevMode = argv[++i];
    else if (a == "--base" && i + 1 < argc) base = argv[++i];
    else if (a == "--swiss-dem" && i + 1 < argc) swissDemPath = argv[++i];
    else { Usage(argv[0]); return 1; }
  }
  if (missionPath.empty() == campaignPath.empty()) { Usage(argv[0]); return 1; }
  if (!campaignPath.empty() && (!statePath.empty() || !carryList.empty())) {
    fprintf(stderr, "fb-gym: --state/--carry belong to a single --mission; a campaign carries its own\n");
    return 1;
  }
  if (threads < 1) { fprintf(stderr, "fb-gym: --threads must be >= 1\n"); return 1; }
  if (!Missions::FBEnsureDir(outDir)) { fprintf(stderr, "fb-gym: cannot create --out %s\n", outDir.c_str()); return 1; }

  static Clients::FBStdoutLogSink gStdoutSink;
  FBLog::SetSink(&gStdoutSink);
  FBLog::SetLevel(FBLogLevel::Debug);

  if (elevMode.empty()) elevMode = FileExists(swissDemPath) ? "swiss" : "const";

  /* This provider needs the mission's runways BEFORE FBRunMission parses the file, and the parser is
   * pure string-in/struct-out — so a second cheap parse beats threading a deferred provider through
   * the runner for one CLI mode. */
  std::unique_ptr<FBElevationProvider> elevation;
  if (elevMode == "tiles") {
    elevation = std::make_unique<World::FBTilesElevation>(base.c_str());
  } else if (elevMode == "swiss") {
    auto baked = std::make_unique<FBBakedDemElevation>(swissDemPath);
    if (!baked->Ok())
      FBLog::Warn("gym", "swiss_dem_load_failed", {{"path", swissDemPath}, {"fallback", "0 m everywhere in the asset's absence"}});
    elevation = std::move(baked);
  } else if (elevMode == "const") {
    /* A campaign's plateau is the union of its steps' runways: the provider is the client's ONE shared
     * object and outlives every single mission in the sequence. */
    std::vector<FBRunway> runways;
    for (const std::string &path : CampaignMissionPaths(campaignPath, missionPath)) {
      std::ifstream in(path);
      if (!in) continue;
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

  if (!campaignPath.empty()) {
    Missions::FBCampaignEnv env{elevMode, swissDemPath, base};
    return Missions::FBRunCampaign(campaignPath, outDir, Missions::FBNativeModelRoots(), *elevation,
                                   (size_t)threads, env);
  }

  /* The standalone replay of a campaign step: the same run the campaign made, from the same text. */
  FBCampaignState carried;
  Missions::FBMissionCarry carry;
  if (!statePath.empty()) {
    std::ifstream in(statePath);
    if (!in) { fprintf(stderr, "fb-gym: cannot open --state %s\n", statePath.c_str()); return 1; }
    std::stringstream buf; buf << in.rdbuf();
    std::string serr;
    if (!FBParseCampaignState(buf.str(), carried, &serr)) {
      fprintf(stderr, "fb-gym: --state %s: %s\n", statePath.c_str(), serr.c_str());
      return 1;
    }
    if (!ParseCarryList(carryList, carry.Mask)) {
      fprintf(stderr, "fb-gym: --carry wants a comma-separated subset of units,ground,stores\n");
      return 1;
    }
    carry.In = &carried;
  }
  return FBRunMission(missionPath, timeout, outDir, Missions::FBNativeModelRoots(), *elevation, nullptr,
                      (size_t)threads, false, statePath.empty() ? nullptr : &carry);
}
