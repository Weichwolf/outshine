#include "FBCampaignRunner.h"
#include "FBCampaignFile.h"
#include "FBCampaignState.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBMissionFile.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace FlightBox::Missions {

namespace {

std::string DirOf(const std::string &path) {
  size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

std::string StemOf(const std::string &path) {
  size_t slash = path.find_last_of('/');
  std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  size_t dot = base.find_last_of('.');
  return dot == std::string::npos ? base : base.substr(0, dot);
}

bool ReadFile(const std::string &path, std::string &out) {
  std::ifstream in(path);
  if (!in) return false;
  std::stringstream buf;
  buf << in.rdbuf();
  out = buf.str();
  return true;
}

bool WriteFile(const std::string &path, const std::string &text) {
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  out << text;
  return out.good();
}

/* The output directory carries the campaign's ORDER in its name, so the fingerprint's file order is
 * the campaign's and never the filesystem's. */
std::string StepName(size_t index, const std::string &missionPath) {
  char idx[8];
  snprintf(idx, sizeof idx, "%02zu", index + 1);
  return idx + ("-" + StemOf(missionPath));
}

FBMissionResult ResultOfExit(int exitCode) {
  switch (exitCode) {
    case 0: return FBMissionResult::Success;
    case 2: return FBMissionResult::Crash;
    case 3: return FBMissionResult::Timeout;
    default: break;
  }
  return FBMissionResult::Fail;
}

struct FBCampaignStep {
  std::string File, Dir;   /* the path AS DECLARED and the step directory's own name */
  int Exit = 0;
};

} // namespace

int FBRunCampaign(const std::string &campaignPath, const std::string &outDir,
                  const FBModelRoots &models, FBElevationProvider &elevation, size_t threads,
                  const FBCampaignEnv &env) {
  std::string text;
  if (!ReadFile(campaignPath, text)) {
    fprintf(stderr, "campaign: cannot open %s\n", campaignPath.c_str());
    return 1;
  }
  FBCampaign campaign;
  std::string perr;
  if (!FBParseCampaignFile(text, campaign, &perr)) {
    fprintf(stderr, "campaign: parse: %s\n", perr.c_str());
    return 1;
  }

  /* Every mission is parsed BEFORE the first one flies: a campaign that dies at step 7 on a typo has
   * wasted six runs. The parsed copies also carry the teams the attrition report needs. */
  const std::string base = DirOf(campaignPath);
  std::vector<std::string> missionPaths;
  std::vector<FBMission> parsed;
  int clocklessMissions = 0;
  for (const std::string &rel : campaign.Missions) {
    const std::string path = rel.empty() || rel[0] == '/' ? rel : base + "/" + rel;
    std::string mtext;
    if (!ReadFile(path, mtext)) {
      fprintf(stderr, "campaign: cannot open mission %s\n", path.c_str());
      return 1;
    }
    FBMission m;
    std::string merr;
    if (!FBParseMissionFile(mtext, m, &merr)) {
      fprintf(stderr, "campaign: mission %s: %s\n", path.c_str(), merr.c_str());
      return 1;
    }
    if (!m.HaveTime) clocklessMissions++;
    missionPaths.push_back(path);
    parsed.push_back(std::move(m));
  }

  if (!FBEnsureDir(outDir)) {
    fprintf(stderr, "campaign: cannot create --out %s\n", outDir.c_str());
    return 1;
  }
  const std::string logPath = outDir + "/campaign.log";
  Clients::FBFileHandle logf = Clients::FBOpenFile(logPath.c_str(), "w");
  if (!logf) {
    fprintf(stderr, "campaign: cannot open %s for writing\n", logPath.c_str());
    return 1;
  }
  Clients::FBFileLogSink fileSink(logf.get());
  Clients::FBStdoutLogSink stdoutSink;
  Clients::FBCompositeLogSink logSink;
  logSink.Add(&fileSink);
  logSink.Add(&stdoutSink);
  /* Every mission installs its OWN sink and clears it on the way out, so the campaign's channel is
   * re-armed before each of its own lines. The scope only guarantees nothing dangles at the end. */
  Clients::FBLogSinkScope logScope(&logSink);
  auto arm = [&]() { FBLog::SetSink(&logSink); FBLog::SetTime(0.0); };

  arm();
  char iso[21];
  FBLog::Info("campaign", "CAMPAIGN_START", {{"name", campaign.Name},
      {"missions", (int)missionPaths.size()}, {"carry", FBCarryMaskStr(campaign.Carry)},
      {"stopOn", FBCampaignStopStr(campaign.StopOn)},
      {"utc", campaign.HaveTime ? FBFormatIsoUtc(campaign.UtcT0S, iso, sizeof iso) : "none"},
      {"elev", env.Elev}, {"out", outDir}});
  /* A campaign whose steps have no declared instant is reproducible only within one day on the two
   * clients that fall back to the wall clock. Said once, at the start, where it can still be fixed. */
  if (!campaign.HaveTime && clocklessMissions > 0)
    FBLog::Warn("campaign", "NO_CLOCK", {{"missions", clocklessMissions},
        {"reason", "neither the campaign nor these missions declare 'time' — a measurement should"}});

  FBCampaignState state;
  std::vector<FBCampaignStep> steps;
  int worst = 0, expended[kFBStoreKinds] = {};
  bool stopped = false;
  for (size_t i = 0; i < missionPaths.size(); i++) {
    const std::string name = StepName(i, missionPaths[i]);
    const std::string dir = outDir + "/" + name;
    if (!FBEnsureDir(dir)) {
      fprintf(stderr, "campaign: cannot create %s\n", dir.c_str());
      return 1;
    }
    FBMissionOutcome outcome;
    outcome.State = state;   /* a unit the overlay dropped has no actor left to re-state its death */
    FBMissionCarry carry;
    carry.In = state.Empty() ? nullptr : &state;
    carry.Mask = campaign.Carry;
    carry.CampaignUtcT0S = campaign.UtcT0S;
    carry.HaveCampaignTime = campaign.HaveTime;
    carry.Out = &outcome;

    const int exitCode = FBRunMission(missionPaths[i], 0.0, dir, models, elevation, nullptr, threads,
                                      false, &carry);
    state = outcome.State;
    for (int k = 0; k < kFBStoreKinds; k++) expended[k] += outcome.ExpendedByKind[k];
    if (!WriteFile(dir + "/campaign-state.txt", state.Format((int)i + 1))) {
      fprintf(stderr, "campaign: cannot write %s/campaign-state.txt\n", dir.c_str());
      return 1;
    }
    /* The summary is an ARTEFACT, so it names the step as the campaign file does and the directory by
     * its own name: it stays true if the output tree is moved or compared against another machine's. */
    steps.push_back({campaign.Missions[i], name, exitCode});
    if (exitCode > worst) worst = exitCode;

    arm();
    FBLog::Info("campaign", "MISSION_RESULT", {{"index", (int)i + 1}, {"mission", missionPaths[i]},
        {"exit", exitCode}, {"result", FBMissionResultStr(ResultOfExit(exitCode))}, {"out", dir}});

    if ((campaign.StopOn == FBCampaignStop::Fail && exitCode != 0) ||
        (campaign.StopOn == FBCampaignStop::Crash && exitCode == 2)) {
      FBLog::Warn("campaign", "STOPPED", {{"after", (int)i + 1},
          {"stopOn", FBCampaignStopStr(campaign.StopOn)}, {"exit", exitCode}});
      stopped = true;
      break;
    }
  }

  arm();
  int lostAir[3] = {}, lostGround[3] = {};
  std::ostringstream summary;
  /* The environment travels WITH the state, because a fingerprint is only comparable between runs over
   * the same ground: a replay that guessed the elevation source would report a divergence that is its
   * own, and a false alarm on this measurement is worse than no measurement. */
  summary << "campaign " << campaign.Name << "\n"
          << "carry " << FBCarryMaskStr(campaign.Carry) << "\n"
          << "stop_on " << FBCampaignStopStr(campaign.StopOn) << "\n"
          /* The campaign CLOCK travels with the environment for the same reason the ground does: a step
           * replayed without it runs under a different sky and reports a divergence that is the
           * replay's own. `none` where the campaign declares none, so the record is always present. */
          << "time " << (campaign.HaveTime ? FBFormatIsoUtc(campaign.UtcT0S, iso, sizeof iso) : "none") << "\n"
          << "elev " << env.Elev << "\n"
          << "dem " << env.Dem << "\n"
          << "base " << env.Base << "\n"
          << "threads " << threads << "\n"
          << "missions " << steps.size() << " of " << missionPaths.size() << "\n";
  for (size_t i = 0; i < steps.size(); i++)
    summary << "mission " << (i + 1) << " " << steps[i].File << " " << steps[i].Exit << " "
            << FBMissionResultStr(ResultOfExit(steps[i].Exit)) << " " << steps[i].Dir << "\n";
  for (const FBCampaignUnitState &u : state.Units()) {
    if (!u.Destroyed) continue;
    FBUnitTeam team = FBUnitTeam::Friendly;
    for (const FBMission &m : parsed) {
      bool found = false;
      for (const FBMissionUnit &b : m.Units)
        if (b.Id == u.Id) { team = b.Team; found = true; break; }
      if (found) break;
    }
    (u.Ground ? lostGround : lostAir)[(int)team]++;
    summary << "lost " << (u.Ground ? "ground " : "unit ") << u.Id << " " << FBUnitTeamStr(team) << "\n";
  }
  for (int k = 0; k < kFBStoreKinds; k++)
    if (expended[k] > 0) summary << "expended " << kStoreCatalogue[k]->Key << " " << expended[k] << "\n";
  summary << "exit " << worst << "\n";

  FBLog::Info("campaign", "ATTRITION", {{"unitsFriendly", lostAir[(int)FBUnitTeam::Friendly]},
      {"unitsHostile", lostAir[(int)FBUnitTeam::Hostile]},
      {"groundFriendly", lostGround[(int)FBUnitTeam::Friendly]},
      {"groundHostile", lostGround[(int)FBUnitTeam::Hostile]}});
  for (int k = 0; k < kFBStoreKinds; k++)
    if (expended[k] > 0)
      FBLog::Info("campaign", "EXPENDED", {{"store", kStoreCatalogue[k]->Key}, {"count", expended[k]}});

  int succeeded = 0, failed = 0, crashed = 0, timedOut = 0;
  for (const FBCampaignStep &s : steps)
    switch (s.Exit) {
      case 0: succeeded++; break;
      case 2: crashed++; break;
      case 3: timedOut++; break;
      default: failed++; break;
    }
  FBLog::Info("campaign", "CAMPAIGN_RESULT", {{"name", campaign.Name}, {"run", (int)steps.size()},
      {"of", (int)missionPaths.size()}, {"succeeded", succeeded}, {"failed", failed},
      {"crashed", crashed}, {"timedOut", timedOut}, {"stopped", stopped}, {"exit", worst}});

  if (!WriteFile(outDir + "/campaign-state.txt", state.Format((int)steps.size())) ||
      !WriteFile(outDir + "/campaign-summary.txt", summary.str())) {
    fprintf(stderr, "campaign: cannot write the summary into %s\n", outDir.c_str());
    return 1;
  }
  return worst;
}

} // namespace FlightBox::Missions
