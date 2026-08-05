/* The layer above a mission: a loop over FBRunMission, not a second engine (doc/missions/campaign.md).
 * It runs the .fbc's missions in file order, carries three monotone facts between them through a text
 * state file, and aggregates their verdicts into one report. Everything it does to a mission it does
 * BEFORE the spawn and through the ordinary runner — which is what keeps every step re-runnable
 * standalone with `fb-gym --mission FILE --state STATE`.
 *
 * NOT the tournament runner (sim/tools/fb_tournament.py): that sweeps INDEPENDENT missions to measure
 * a pilot, this one runs a DEPENDENT sequence to measure a force. The two must stay apart. */
#ifndef FBCAMPAIGNRUNNER_H
#define FBCAMPAIGNRUNNER_H

#include <string>
#include "FBElevationProvider.h"
#include "FBMissionRunner.h"
#include "FBModelRoots.h"

namespace FlightBox::Missions {

/* WHICH GROUND the campaign was flown over, in the client's own words. The runner takes an injected
 * FBElevationProvider and could not name it if it wanted to — but a fingerprint is only comparable
 * between runs over the SAME ground, so the choice must be written down beside the state instead of
 * living in the operator's memory. It is the only client switch that moves a campaign's result:
 * weather is declared in the mission, the clock in the mission or the .fbc, the timeout is the file's
 * (a campaign never overrides it), and `--threads` is measured result-neutral. */
struct FBCampaignEnv {
  std::string Elev;       /* the RESOLVED --elev mode, never the empty default */
  std::string Dem;        /* the baked asset path — decides the ground under `baked` */
  std::string Base;       /* the tile server — decides the ground under `tiles` */
};

/* Runs every mission of `campaignPath` into outDir/NN-<missionfile>/ and writes the state file, the
 * campaign log and the machine-readable summary beside them. Returns the WORST mission's exit code
 * (0/1/2/3), or 1 for a campaign that does not parse — with the same reading rule a combat mission
 * carries: the verdict is the ATTRITION and MISSION_RESULT lines, not the code. */
int FBRunCampaign(const std::string &campaignPath, const std::string &outDir,
                  const FBModelRoots &models, FBElevationProvider &elevation, size_t threads,
                  const FBCampaignEnv &env);

} // namespace FlightBox::Missions
#endif
