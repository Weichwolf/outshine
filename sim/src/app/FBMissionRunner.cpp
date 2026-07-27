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
#include "FBGeodesy.h"
#include "FBGunBallistics.h"
#include "FBGunProjectiles.h"
#include "FBStore.h"
#include "FBUnits.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <cmath>
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
  for (const auto &a : actors) {
    /* A weapon's K.O. is its IMPACT — the event the mission was flown for, not the end of it. It is
     * judged by the very same FBFlightMonitor as every other unit (the ground contact, the speed, the
     * attitude are the same physics); only the CONSEQUENCE differs, and that consequence belongs to the
     * owner of the simulation, which is this file. See RetireImpactedStores below. */
    if (a->GetKind() != FBUnitKind::Aircraft) continue;   /* a store's K.O. is its impact; a ground
                                                           * target has no flight to lose */
    if (a->FlightMonitor().Tripped()) return a.get();
  }
  return nullptr;
}

/* ---- THE COMBINATION RULE, and the one thing objectives changed about it ----
 * An actor's loss is EXPECTED when it is another actor's DECLARED OBJECTIVE (core/FBObjective.h): this
 * unit is combat-ineffective, and somebody else's mission file said in so many words that making it so
 * was the point. Two observed facts and a declaration — no team heuristic, no notion of "the player's
 * side", and nothing at all for a mission that declares no objectives, which is why every legacy
 * mission combines exactly as it always did.
 *
 * WHY IT EXISTS: a duel has a winner and a loser, not two failures. Before objectives, the loser's FAIL
 * was the only verdict in the run and it became the run's — so a mission whose HOSTILE unit was shot
 * down reported FAIL, and the shooter's success was invisible. An expected loss is still reported as
 * that actor's own FAIL in its UNIT_RESULT line (it lost, and the record says so); what it no longer
 * does is decide the run. */
bool ExpectedLoss(const FBActorList &actors, const FBSimUnit &a) {
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

/* The MISSION verdict is per actor and combined here, nowhere else: an actor is JUDGED iff the mission
 * gave it objectives (it then carries an FBMissionMonitor, app/FBMissionBoot.h). The run is over the
 * moment ONE judged actor's failure DECIDES it (there is nothing left to prove) — an expected loss does
 * not, so the run goes on until the side that declared it has an answer too. */
const FBSimUnit *FirstDecidingFailure(const FBActorList &actors) {
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (!m || !m->Concluded() || m->Verdict() == FBMissionVerdict::Success) continue;
    if (!ExpectedLoss(actors, *a)) return a.get();
  }
  return nullptr;
}
/* ...and it is over anyway once every judged actor has an answer, whatever that answer is. For a
 * mission with no objectives that is the same instant as "every judged actor succeeded", because there
 * any failure at all is a deciding one and stops the loop above first. */
bool AllJudgedConcluded(const FBActorList &actors) {
  bool anyJudged = false;
  for (const auto &a : actors) {
    const FBMissionMonitor *m = a->MissionMonitor();
    if (!m) continue;
    anyJudged = true;
    if (!m->Concluded()) return false;
  }
  return anyJudged;
}
/* The judged actor whose verdict/detail the combined RESULT quotes once nothing decided the run — the
 * first one, matching the single-actor case exactly (there the primary IS the only judged actor). An
 * actor whose loss was somebody's declared objective is skipped: quoting the LOSER of a decided duel as
 * the run's verdict is precisely the team-blindness objectives exist to remove. If every judged actor
 * lost that way (a mutual exchange), the first of them speaks after all — nobody came home, and the
 * record should say so rather than invent a winner. */
const FBSimUnit *FirstJudged(const FBActorList &actors) {
  for (const auto &a : actors)
    if (a->MissionMonitor() && !ExpectedLoss(actors, *a)) return a.get();
  for (const auto &a : actors)
    if (a->MissionMonitor()) return a.get();
  return nullptr;
}


/* ---- Released stores: the actor list's ONE runtime growth path ----
 * A store becomes a unit at the END of the tick its release was commanded in, so it is only stepped
 * from the NEXT tick onwards. That is not a convenience: the step phase runs one job per actor index
 * (app/FBTickPool.h), and an actor appearing mid-phase would make a run's outcome depend on WHEN in the
 * phase it appeared. Appending at the barrier keeps the whole tick a snapshot, exactly like every pose.
 *
 * The order is deterministic all the way down: actors are drained in list order, each actor's own
 * release queue is FIFO, and every new actor is appended in that order — so the list, the tick order
 * and the unit ids are identical in a 1-thread and an N-thread run. */
struct FBStoreTrack {
  size_t Index = 0;      /* into the actor list */
  double SpawnS = 0.0;
  double DeadlineS = 0.0;   /* SpawnS + the store's own MaxFlightS (core/FBStore.h) */
  const FBStoreSpec *Spec = nullptr;
  int    LauncherId = 0;
  /* WHAT THE COMPUTER SAID WOULD HAPPEN (core/FBStore.h's FBReleaseSolution), carried out of the jet on
   * the round itself. It is here so that the impact — which this file measures — can be reported beside
   * the prediction it is the answer to, in one line, by the only thing that knows both. */
  FBReleaseSolution Solution;
  /* Closest this store came to any aircraft OTHER THAN THE ONE THAT LAUNCHED IT — the number that says
   * how the shot went. The launcher is excluded from the REPORT and not from the fuze (below): a round
   * separating from a pylon passes its own carrier at tens of metres, which is a fact about geometry and
   * not about aim. */
  double MinMissM = 1e18;
  int    MinMissUnit = 0;
};

/* ---- The proximity fuze: WAS THIS A HIT? ----
 * A guided round's flight ends where it passes a unit closer than its own fuze radius (core/FBStore.h).
 * That verdict belongs HERE, to the owner of the simulation, for exactly the reason the two monitors do:
 * the missile's own seeker says where it THINKS the target is, and letting the weapon score itself on
 * its own estimate would be the purest form of cheating. This is measured on the published poses, i.e.
 * on the truth, like FBFlightMonitor's ground contact.
 *
 * WHY A CLOSEST-APPROACH COMPUTATION AND NOT A DISTANCE TEST. The run's tick is 0.1 s and a head-on
 * closure can exceed 1,500 m/s, so consecutive samples are 150 m apart: a plain per-tick distance test
 * against a 10 m fuze radius would miss nearly every real hit. So the miss distance is the minimum over
 * the SEGMENT between the last tick's relative position and this one's — the standard CPA formula on
 * p(t) = p0 + t*(p1-p0), t in [0,1]. The straight-line assumption inside one tick is worth about a
 * metre of curvature at 20 g, which is stated here rather than hidden.
 *
 * THE ARMING DELAY IS WHAT KEEPS A LAUNCH FROM DETONATING ON ITS OWN LAUNCHER: for the first ArmingS
 * seconds the fuze is not live, which is both real and the reason a round leaving a pylon 3 m from the
 * jet that carried it does not count as a hit on it. */
struct FBCpa {
  double MissM = 1e18;
  double ClosureMs = 0.0;
  double FracT = 0.0;   /* where inside the tick the burst happened, 0..1 — reported, so the event's
                         * time is the sub-tick one and not the sample it was found in */
  /* WHERE the burst was, relative to the target, in local ENU metres at the moment of closest approach.
   * The same vector whose length is MissM — carried out because a damage resolution needs the DIRECTION
   * too (core/FBDamageModel works in the target's body frame), and recomputing it anywhere else would be
   * a second, drifting copy of the same geometry. */
  double RelE = 0.0, RelN = 0.0, RelU = 0.0;
};

FBCpa ClosestApproach(const FBUnitPose &a0, const FBUnitPose &b0, const FBUnitPose &a1,
                      const FBUnitPose &b1, double dt) {
  double p0e = 0.0, p0n = 0.0, p1e = 0.0, p1n = 0.0;
  FBEnuOffsetM(b0.LatDeg, b0.LonDeg, a0.LatDeg, a0.LonDeg, p0e, p0n);
  FBEnuOffsetM(b1.LatDeg, b1.LonDeg, a1.LatDeg, a1.LonDeg, p1e, p1n);
  double p0u = a0.ElevM - b0.ElevM, p1u = a1.ElevM - b1.ElevM;
  double de = p1e - p0e, dn = p1n - p0n, du = p1u - p0u;
  double denom = de * de + dn * dn + du * du;
  double t = denom > 1e-9 ? -(p0e * de + p0n * dn + p0u * du) / denom : 0.0;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  double me = p0e + t * de, mn = p0n + t * dn, mu = p0u + t * du;
  FBCpa c;
  c.MissM = std::sqrt(me * me + mn * mn + mu * mu);
  c.ClosureMs = dt > 0.0 ? std::sqrt(denom) / dt : 0.0;
  c.FracT = t;
  c.RelE = me; c.RelN = mn; c.RelU = mu;
  return c;
}

/* ---- THE HIT'S CONSEQUENCE: geometry -> damage ----
 * The burst is a point in the world; what it wrecked depends on where that point sits on the TARGET'S
 * airframe, so the closest-approach vector is rotated into the target's body frame and handed to
 * core/FBDamageModel through the unit that owns the health register (units/FBSimUnit::TakeBurst).
 *
 * WHY HERE. The same reason the fuze verdict above is here and not in the weapon: the owner of the
 * simulation resolves what happened between two units, on the published poses, i.e. on the truth. The
 * WEAPON supplies one number (its warhead mass, core/FBStore.h), the TARGET'S MODULE supplies one table
 * (where its systems are, FBModule::DamageLayout) and neither of them decides anything. The attitude
 * used is the target's published pose — the same snapshot everything else this tick was measured
 * against, so the whole resolution is a function of already-observed state and of nothing else. */
void ResolveBurst(FBSimUnit &target, const FBCpa &c, const FBStoreSpec &spec) {
  const FBUnitPose &p = target.GetPose();
  FBBurst b;
  FBEnuToBodyVec(p.RollDeg, p.PitchDeg, p.YawDeg, c.RelE, c.RelN, c.RelU, b.FwdM, b.RightM, b.DownM);
  b.ClosureMs = c.ClosureMs;
  b.WarheadKg = spec.WarheadKg;
  FBDamageResult r = target.TakeBurst(b);
  FBLogUnitScope us(target.LogLabel());
  FBLog::Info("damage", "DAMAGE", {{"zone", FBDamageZoneStr(r.Zone)}, {"rangeM", r.RangeM},
      {"fluxJm2", r.PeakFluxJm2}, {"warheadKg", spec.WarheadKg}, {"closureMs", c.ClosureMs},
      {"bodyFwdM", b.FwdM}, {"bodyRightM", b.RightM}, {"bodyDownM", b.DownM},
      {"failed", (int)r.NewlyFailed}, {"degraded", (int)r.NewlyDegraded},
      {"hits", target.Health().Hits()}});
  /* One line per system that changed state, so a damage picture is greppable rather than a bitmask to
   * decode by hand. */
  for (int i = 0; i < (int)FBSystemId::Count; i++) {
    uint32_t bit = 1u << i;
    if (!((r.NewlyFailed | r.NewlyDegraded) & bit)) continue;
    FBLog::Warn("damage", "SYSTEM", {{"system", FBSystemIdStr((FBSystemId)i)},
        {"state", FBHealthStateStr((r.NewlyFailed & bit) ? FBHealthState::Failed
                                                         : FBHealthState::Degraded)}});
  }
  if (r.WasEffective && !r.NowEffective)
    FBLog::Warn("damage", "KILL", {{"reason", "combat ineffective"},
        {"failed", (int)target.Health().FailedMask()}, {"altM", p.ElevM}, {"speedMs", p.SpeedMs}});
}

/* Below this many expected rounds a pass is not a hit but a near miss: the density model is continuous
 * and would otherwise report a millionth of a round every time a bundle flew past anything. A tenth of a
 * round is comfortably under one hit and comfortably over the noise. */
constexpr double kMinReportedHits = 0.1;
/* How far off the axis a bundle can pass and still be worth resolving, on top of three sigma of its own
 * pattern: the airframe's own reach, i.e. a fighter's half span. Not a hit radius — the density model
 * decides that — but the point past which it can only return zero. */
constexpr double kGunHitReachM = 8.0;
/* ...and how close a bundle's CLOSEST approach has to be to be worth a line in the record at all. Wide
 * enough that a burst that "went past him" is measured, narrow enough that a bundle crossing the same
 * sky is not. */
constexpr double kGunNearMissM = 200.0;

/* ---- THE GUN'S HALF OF THE SAME JOB: bundle geometry -> damage ----
 * Structurally identical to ResolveBurst above and here for exactly the same reason: the owner of the
 * simulation resolves what happened between two units, on the published poses. What differs is what
 * arrives. A warhead is a mass and the model derives an energy from it; a burst of gunfire is a COUNT of
 * rounds in a pattern, so the energy density is computed here — from the miss distance, the pattern's
 * spread at that range, the relative speed at impact and the area the target presents to the stream —
 * and the damage model is handed the result (core/FBDamageModel's FBKineticBurst).
 *
 * The rounds hit an AREA of the airframe, not a point, so the footprint's centre along the target's own
 * axis decides which zones see anything at all — the same body-frame rotation the fragment path uses,
 * of the same closest-approach vector. */
bool ResolveGunHit(FBSimUnit &target, const FBCpa &c, const FBGunProjectiles::Bundle &bundle,
                   double sigmaM, double relSpeedMs) {
  const FBUnitPose &p = target.GetPose();
  double fwd = 0.0, right = 0.0, down = 0.0;
  FBEnuToBodyVec(p.RollDeg, p.PitchDeg, p.YawDeg, c.RelE, c.RelN, c.RelU, fwd, right, down);
  const FBDamageLayout &layout = target.Module().DamageLayout();
  /* WHAT THE STREAM SEES: the presented area for the direction it arrived from — the same relative
   * geometry, expressed in the target's own frame (core/FBDamageModel::FBPresentedAreaM2). */
  double areaM2 = FBPresentedAreaM2(layout, fwd, right, down);
  if (areaM2 <= 0.0) return false;   /* nothing to hit: a store, a unit with no declared airframe */
  double extentM = FBPresentedExtentM(layout, fwd, right, down);

  double hits = FBGunExpectedHits(bundle.Rounds, c.MissM, sigmaM, areaM2, extentM);
  double flux = FBGunFluxJm2(bundle.Rounds, *bundle.Spec, relSpeedMs, c.MissM, sigmaM, areaM2, extentM);
  /* The pattern passed close but the density model puts no rounds on him: that is a MISS, and it is the
   * caller's to record — this function reports only what it actually resolved. */
  if (hits < kMinReportedHits) return false;

  FBKineticBurst kb;
  kb.FwdM = fwd;
  kb.FluxJm2 = flux;
  kb.SpreadM = sigmaM;
  kb.Rounds = hits;
  kb.ImpactSpeedMs = relSpeedMs;
  FBDamageResult r = target.TakeKineticBurst(kb);
  FBLogUnitScope us(target.LogLabel());
  FBLog::Info("gun", "HIT", {{"rounds", hits}, {"ofRounds", bundle.Rounds}, {"missM", c.MissM},
      {"spreadM", sigmaM}, {"impactMs", relSpeedMs}, {"areaM2", areaM2}, {"extentM", extentM},
      {"fluxJm2", flux},
      {"zone", FBDamageZoneStr(r.Zone)}, {"bodyFwdM", fwd}, {"bodyRightM", right},
      {"bodyDownM", down}, {"failed", (int)r.NewlyFailed}, {"degraded", (int)r.NewlyDegraded},
      {"hitsTotal", target.Health().Hits()}});
  for (int i = 0; i < (int)FBSystemId::Count; i++) {
    uint32_t bit = 1u << i;
    if (!((r.NewlyFailed | r.NewlyDegraded) & bit)) continue;
    FBLog::Warn("damage", "SYSTEM", {{"system", FBSystemIdStr((FBSystemId)i)},
        {"state", FBHealthStateStr((r.NewlyFailed & bit) ? FBHealthState::Failed
                                                         : FBHealthState::Degraded)}});
  }
  if (r.WasEffective && !r.NowEffective)
    FBLog::Warn("damage", "KILL", {{"reason", "combat ineffective"},
        {"failed", (int)target.Health().FailedMask()}, {"altM", p.ElevM}, {"speedMs", p.SpeedMs}});
  return true;
}

/* ---- THE GROUND BURST: what a bomb does where it lands ----
 * The unguided counterpart of ResolveBurst above, and the same rule end to end: the OWNER of the
 * simulation resolves what happened between two units, on observed truth. The store supplies its warhead
 * mass and the speed it arrived at (core/FBStore.h), the TARGET's module supplies where its structure is
 * (FBModule::DamageLayout), core/FBDamageModel does the rest — the same 1/r^2 fragment law an aircraft is
 * judged by, so a bomb near-missing a bunker and a missile near-missing a fighter go through one model.
 *
 * WHAT IT DELIBERATELY DOES NOT REACH: aircraft. A jet low over its own detonation is inside a real
 * fragment envelope, and modelling it would need a frag-vs-airframe geometry (and an escape manoeuvre to
 * measure it against) that nothing here has; resolving a burst against everything within an invented
 * cutoff would be a number pretending to be physics. So a ground burst hurts GROUND units, and that
 * boundary is stated rather than hidden behind a radius constant. */
bool ResolveGroundBurst(FBSimUnit &target, const fb_fdm_state &burst, const FBStoreSpec &spec) {
  const FBUnitPose &p = target.GetPose();
  double relE = 0.0, relN = 0.0;
  FBEnuOffsetM(p.LatDeg, p.LonDeg, burst.lat, burst.lon, relE, relN);
  double relU = burst.elev - p.ElevM;
  /* IS THIS BURST EVEN NEAR HIM? The gate is DERIVED, not a radius somebody picked: the lowest threshold
   * this target's own layout declares is the least energy that can do anything to it at all, so a burst
   * whose flux at this distance is below that could only ever produce a zero-effect DAMAGE line and a
   * spurious entry in his hit count. A bomb landing 14 km away is not a hit on him in any sense. */
  double dist = std::sqrt(relE * relE + relN * relN + relU * relU);
  const FBDamageLayout &layout = target.Module().DamageLayout();
  double least = 0.0;
  for (int i = 0; i < layout.ZoneCount; i++)
    for (int k = 0; k < layout.Zones[i].SystemCount; k++) {
      double th = layout.Zones[i].Systems[k].DegradeJm2;
      if (th > 0.0 && (least == 0.0 || th < least)) least = th;
    }
  if (least > 0.0 && FBFragmentFluxJm2(spec.WarheadKg, dist, burst.speed) < least) return false;

  FBBurst b;
  FBEnuToBodyVec(p.RollDeg, p.PitchDeg, p.YawDeg, relE, relN, relU, b.FwdM, b.RightM, b.DownM);
  b.ClosureMs = burst.speed;   /* the round's own arrival speed: the target is not moving */
  b.WarheadKg = spec.WarheadKg;
  FBDamageResult r = target.TakeBurst(b);
  FBLogUnitScope us(target.LogLabel());
  FBLog::Info("damage", "DAMAGE", {{"zone", FBDamageZoneStr(r.Zone)}, {"rangeM", r.RangeM},
      {"fluxJm2", r.PeakFluxJm2}, {"warheadKg", spec.WarheadKg}, {"closureMs", b.ClosureMs},
      {"bodyFwdM", b.FwdM}, {"bodyRightM", b.RightM}, {"bodyDownM", b.DownM},
      {"failed", (int)r.NewlyFailed}, {"degraded", (int)r.NewlyDegraded},
      {"hits", target.Health().Hits()}});
  for (int i = 0; i < (int)FBSystemId::Count; i++) {
    uint32_t bit = 1u << i;
    if (!((r.NewlyFailed | r.NewlyDegraded) & bit)) continue;
    FBLog::Warn("damage", "SYSTEM", {{"system", FBSystemIdStr((FBSystemId)i)},
        {"state", FBHealthStateStr((r.NewlyFailed & bit) ? FBHealthState::Failed
                                                         : FBHealthState::Degraded)}});
  }
  if (r.WasEffective && !r.NowEffective)
    FBLog::Warn("damage", "KILL", {{"reason", "structure destroyed"},
        {"failed", (int)target.Health().FailedMask()}, {"lat", p.LatDeg}, {"lon", p.LonDeg}});
  return true;
}

/* The impact report: what the store was doing at the moment its own FBFlightMonitor said it hit
 * something. Everything here is OBSERVED — position and velocity out of the FDM state, the reason out
 * of the judge — so a detonation is measured, never scripted. `mode` separates the two ways a store's
 * flight can end: a ground contact (the detonation) and everything else (a lost weapon), because the
 * monitor can also trip on a tumbling store or a diverged integration and calling that an impact would
 * be a lie in the telemetry. */
/* WHERE THE ROUND CROSSED THE GROUND, as opposed to where it was first SEEN below it. The judge runs on
 * the run's 0.1 s tick, so by the time a store is observed to have penetrated it has already travelled
 * up to a tick past the surface — measured on a Mk-82 arriving at 216 m/s: 14 m of penetration, i.e.
 * ~20 m of horizontal travel beyond the true impact point. That is a fifth of the whole delivery error
 * this mission set exists to measure, and it is an artefact of the sampling rate rather than anything
 * the aircraft or the computer did.
 *
 * So the crossing is recovered from the observed sample itself — exactly what the proximity fuze already
 * does for an air burst (ClosestApproach's FracT: "the event's time is the sub-tick one and not the
 * sample it was found in"). The store is `depth` metres under the surface with a known velocity and NO
 * contact forces acting on it (a weapon's airframe is deliberately given no ground to collide with,
 * units/FBSimUnit's kWeaponNoGroundElevM), so it is still on its ballistic arc and the crossing is
 * depth / sink-rate seconds behind: back-project the position by that. Over ~0.1 s the arc's curvature is
 * worth centimetres, which is why this is a straight line and not a second integration.
 *
 * Deliberately NOT interpolated between the last two published POSES: by the time the physics judge
 * concludes, the previous pose is already under the surface too (it takes a few metres of penetration to
 * be a conclusion rather than a rounding error), so there is no bracketing pair to interpolate between.
 *
 * Everything downstream — the impact record, the delivery error and the burst the damage model resolves
 * — uses THIS point. `backS` is how far behind the observed sample it was. */
fb_fdm_state GroundCrossing(const FBSimUnit &store, double &backS) {
  fb_fdm_state s = store.State();
  backS = 0.0;
  double depth = store.GroundAslM() - s.elev;
  if (!(depth > 0.0) || !(s.vy < -0.1)) return s;
  backS = depth / -s.vy;
  double coslat = std::cos(s.lat * kDeg2Rad);
  /* fb_fdm_state velocity is X-Plane local: +x east, +y up, +z south (fdm/FBFdm.h). */
  s.lat -= (-s.vz) * backS / kMPerDeg;
  s.lon -= coslat > 1e-6 ? s.vx * backS / (kMPerDeg * coslat) : 0.0;
  s.elev = store.GroundAslM();
  return s;
}

void LogStoreImpact(const FBSimUnit &store, const FBStoreTrack &track, double simT,
                    const fb_fdm_state &cross, double backS) {
  const fb_fdm_state &st = store.State();
  double horizMs = std::sqrt(st.vx * st.vx + st.vz * st.vz);
  double angleDeg = std::atan2(-st.vy, horizMs > 1e-6 ? horizMs : 1e-6) * kRad2Deg;
  FBKoReason r = store.FlightMonitor().Reason();
  bool ground = r == FBKoReason::StructureContact || r == FBKoReason::CfitPenetration ||
                r == FBKoReason::GearUpContact || r == FBKoReason::HardLanding ||
                r == FBKoReason::AttitudeContact;
  FBLog::Info("stores", "IMPACT", {{"mode", ground ? "ground" : "lost"},
      {"reason", FBKoReasonStr(r)}, {"lat", st.lat}, {"lon", st.lon}, {"altM", st.elev},
      {"groundAslM", store.GroundAslM()}, {"tofS", simT - track.SpawnS},
      {"speedMs", st.speed}, {"vsMs", st.vy}, {"impactAngleDeg", angleDeg},
      {"pitchDeg", st.pitch}, {"rollDeg", st.roll}, {"trackDeg", st.yaw},
      /* ...and the interpolated crossing the delivery is actually measured against (see
       * GroundCrossing): where it went through the surface, and how far into this tick that was. */
      {"crossLat", cross.lat}, {"crossLon", cross.lon}, {"crossBackS", backS},
      {"crossTofS", simT - track.SpawnS - backS}});
  /* THE DELIVERY, MEASURED: the fire control's prediction (carried out of the jet on the round, see
   * FBStoreTrack::Solution) against the impact just observed. `predErrM` is what the COMPUTER got wrong
   * — the coarse stored table against the model's own aerodynamics, which is the error the whole
   * CCIP/CCRP arrangement exists to expose — and `aimErrM` is what the DELIVERY got wrong, i.e. the two
   * of them plus whatever the release moment cost. Only for a ground contact: a lost store's last
   * position says nothing about aiming. */
  if (ground && track.Solution.Valid) {
    const FBReleaseSolution &sol = track.Solution;
    double predErr = FBPlanarDistM(sol.ImpactLatDeg, sol.ImpactLonDeg, cross.lat, cross.lon);
    double aimErr = FBPlanarDistM(sol.AimLatDeg, sol.AimLonDeg, cross.lat, cross.lon);
    /* Long/short and left/right, in the ROUND'S OWN arrival direction (a bomb weathercocks into its
     * velocity, so its heading at impact is the run-in track): + along = it went long. */
    double alongM = 0.0, acrossM = 0.0;
    FBTrackProjectM(sol.AimLatDeg, sol.AimLonDeg, st.yaw, cross.lat, cross.lon, alongM, acrossM);
    FBLog::Info("stores", "DELIVERY", {{"mode", FBDeliveryModeStr(sol.Mode)},
        {"predLat", sol.ImpactLatDeg}, {"predLon", sol.ImpactLonDeg},
        {"aimLat", sol.AimLatDeg}, {"aimLon", sol.AimLonDeg},
        {"predErrM", predErr}, {"aimErrM", aimErr},
        {"aimLongM", alongM}, {"aimAcrossM", acrossM},
        {"predTofS", sol.TofS}, {"tofS", simT - track.SpawnS - backS},
        {"tofErrS", sol.TofS - (simT - track.SpawnS - backS)},
        {"planeM", sol.ImpactElevM}, {"groundAslM", store.GroundAslM()},
        {"armMarginS", sol.ArmMarginS}, {"solAgeS", track.SpawnS - sol.StampS}});
  }
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

/* WAS THE GROUND THE CAUSE OR THE CONSEQUENCE? A jet that was shot down and then flew into the terrain
 * has TWO true verdicts, and the useful one is the first: the shootdown explains the crash, the crash
 * explains nothing. So the physical judge steps aside for a unit whose mission judge already concluded
 * and which is combat-ineffective — the only way that pairing arises. An undamaged wreck (CFIT, a
 * departure) still reports as one, unchanged. */
bool ShotDownFirst(const FBSimUnit &a) {
  const FBMissionMonitor *m = a.MissionMonitor();
  return m && m->Concluded() && !a.Health().CombatEffective();
}

/* This actor's own result string for the per-unit breakdown: the physical judge outranks the mission
 * judge (a wreck has no mission verdict worth quoting) EXCEPT after a shootdown, see above; an actor
 * without objectives reports NONE. */
const char *ActorResultStr(const FBSimUnit &a) {
  /* A store's physical K.O. is the outcome it was released for, so it is named as such rather than as a
   * crash — same judge, same verdict, different word for a different kind of unit. */
  if (a.GetKind() == FBUnitKind::Weapon)
    return a.FlightMonitor().Tripped() ? "IMPACT" : "IN_FLIGHT";
  /* A ground target has neither judge's question to answer: it cannot crash and it was given nothing to
   * fly. What it CAN report is the only thing that ever happens to it. */
  if (a.GetKind() == FBUnitKind::Ground)
    return a.Health().CombatEffective() ? "INTACT" : "DESTROYED";
  const FBMissionMonitor *m = a.MissionMonitor();
  if (a.FlightMonitor().Tripped() && !ShotDownFirst(a))
    return a.FlightMonitor().Reason() == FBKoReason::Loc ? "LOC" : "CRASH";
  return m ? FBMissionVerdictStr(m->Verdict()) : "NONE";
}
std::string ActorReason(const FBSimUnit &a) {
  if (a.GetKind() == FBUnitKind::Ground) {
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
  FBActorStepJob(FBActorList &actors, const FBUnitRegistry &units,
                 std::vector<FBBufferedLogSink> &logs, double dt)
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
  FBActorList &Actors_;
  const FBUnitRegistry &Units_;
  std::vector<FBBufferedLogSink> &Logs_;
  double Dt_;
  double TimeS_ = 0.0;
};
} // namespace

int FBRunMission(const std::string &missionPath, double timeoutOverride, const std::string &outDir,
                 const FBModelRoots &models, FBElevationProvider &elevation,
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
  /* Capacity for the whole cast INCLUDING every store the mission could release: the list is the one
   * thing in the tick path allowed to grow at runtime (see FBStoreTrack above), and reserving it here
   * means that growth never reallocates while the run is under way. The loaded count is known only
   * after the actors exist, so the reserve is done in two steps — this one for the declared units, the
   * exact one right after the spawn loop. */
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
    std::unique_ptr<FBSimUnit> unit = FBMissionSpawnActor(models, mission, i, groundAsl, timeoutS, &serr);
    if (!unit) {
      FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", serr}});
      return 1;
    }
    Actors.push_back(std::move(unit));
  }

  /* The exact ceiling, now that every module's loadout is applied: one further actor per loaded
   * station, and not one more — a store can be released once. */
  size_t maxActors = Actors.size();
  for (const auto &a : Actors) maxActors += (size_t)a->Module().Stores().LoadedCount();
  Actors.reserve(maxActors);
  std::vector<FBStoreTrack> StoreTracks;
  StoreTracks.reserve(maxActors - Actors.size());
  /* THE ROUNDS IN THE AIR (core/FBGunProjectiles): the client's, like the fuze verdict and the damage
   * resolution, and a fixed pool — a gun engagement adds no allocation to the tick path. */
  FBGunProjectiles Bullets;
  /* ONE record per bundle slot: how close it ever came and to whom. The pool flies the rounds and knows
   * nothing about units; this is the OWNER's book, kept beside it and emitted when the bundle's life
   * ends — the same shape as FBStoreTrack::MinMissM, for the same reason. */
  struct FBGunPass {
    bool   Live = false;
    double MinMissM = 1e18;
    double SpreadM = 0.0, PathM = 0.0, ClosureMs = 0.0, Rounds = 0.0;
    std::string TargetName;
  };
  std::vector<FBGunPass> GunPasses(FBGunProjectiles::kMaxBundles);
  /* LAST TICK'S poses, for the fuze's closest-approach computation (see ClosestApproach's banner). Sized
   * for the ceiling so a store joining the list mid-run never reallocates it, and captured at the very
   * end of every tick — including for a store that only just appeared, whose spawn pose is already
   * published (units/FBSimUnit's constructor). */
  std::vector<FBUnitPose> PrevPose(maxActors);
  bool HavePrevPose = false;

  /* The run's ONE unit registry (units/FBUnitRegistry): the whole cast, in mission-declaration order,
   * filled once now that every actor exists and borrowed by everything that observes units — the
   * modules' sensors through the step job below, and (native only) the hook's FBWorld for drawing. */
  FBUnitRegistry UnitReg;
  for (const auto &a : Actors) UnitReg.Register(a.get());

  /* The OBSERVED roster a combat objective is judged against (core/FBObjective.h), rebuilt once per
   * tick and shown to every judged actor's monitor: callsign, faction, and the one bit that actor's own
   * health register publishes. Weapons are left out — a round in the air is not somebody's target.
   * Reserved for the ceiling, so the tick path never allocates and the borrowed FBMissionRoster view
   * never dangles; the Id borrows the unit's own name, which outlives the run. */
  std::vector<FBUnitObservation> RosterBuf;
  RosterBuf.reserve(maxActors);
  auto BuildRoster = [&]() {
    RosterBuf.clear();
    for (const auto &a : Actors) {
      if (a->GetKind() == FBUnitKind::Weapon) continue;
      RosterBuf.push_back({a->GetName().c_str(), a->GetTeam(), a->Health().CombatEffective()});
    }
    return FBMissionRoster{RosterBuf.data(), (int)RosterBuf.size()};
  };

  if (hook) hook->OnMissionStart(mission.Units.front().Spawn, Actors, UnitReg);

  std::vector<FBActorTelemetry> ActorTelemetry;
  ActorTelemetry.reserve(maxActors);
  ActorTelemetry.resize(Actors.size());
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
  /* Sized for the CEILING, not for today's cast: the job indexes this vector by actor index, and a
   * store joining the list mid-run must not resize a buffer a worker thread is holding a reference to. */
  std::vector<FBBufferedLogSink> actorLogs(maxActors);
  FBActorStepJob stepJob(Actors, UnitReg, actorLogs, dt);
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
  while (!FirstFlightKo(Actors) && !FirstDecidingFailure(Actors) && !AllJudgedConcluded(Actors) &&
         simT < timeoutS) {
    for (auto &a : Actors)
      if (a->Active()) a->UpdateGroundAsl(elevation.GroundElevM(a->State().lat, a->State().lon));
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
      a->RunMonitors(simT, roster);
    }
    for (auto &a : Actors)
      if (a->Active()) a->SampleTelemetry(simT);
    /* ---- THE ROUNDS IN THE AIR (core/FBGunProjectiles) ----
     * Stepped here, in the same pass and for the same reason the fuze is resolved here: they are the
     * client's, they fly on nobody's estimate, and what they hit is measured on the published poses.
     * The order inside the tick is fixed: fly them, then resolve them against every aircraft they
     * passed, then take on whatever the modules fired during this tick (below, with the store releases)
     * so a bundle is never resolved in the tick it was created in — the same snapshot discipline the
     * actor list's growth follows. */
    Bullets.Step(dt);
    if (HavePrevPose) {
      for (int bi = 0; bi < Bullets.Capacity(); bi++) {
        const FBGunProjectiles::Bundle &bundle = Bullets.At(bi);
        FBGunPass &pass = GunPasses[bi];
        if (!bundle.Live) {
          /* THE BUNDLE'S LIFE IS OVER — and the record of how it went is the CLOSEST it ever came, not
           * the first tick it came anywhere near. A burst's debrief turns on that number: it is what
           * says whether the aiming or the timing was wrong. */
          if (pass.Live && pass.MinMissM < kGunNearMissM) {
            FBLog::Info("gun", "MISS", {{"target", pass.TargetName}, {"missM", pass.MinMissM},
                {"spreadM", pass.SpreadM}, {"rounds", pass.Rounds}, {"pathM", pass.PathM},
                {"closureMs", pass.ClosureMs}});
          }
          pass = FBGunPass{};
          continue;
        }
        if (!pass.Live) { pass = FBGunPass{}; pass.Live = true; }
        FBUnitPose b0{}, b1{};
        b0.LatDeg = bundle.PrevLatDeg; b0.LonDeg = bundle.PrevLonDeg; b0.ElevM = bundle.PrevAltM;
        b1.LatDeg = bundle.LatDeg; b1.LonDeg = bundle.LonDeg; b1.ElevM = bundle.AltM;
        for (size_t k = 0; k < Actors.size(); k++) {
          FBSimUnit &tgt = *Actors[k];
          if (tgt.GetKind() != FBUnitKind::Aircraft || !tgt.Active()) continue;
          if (tgt.GetId() == bundle.LauncherId) continue;   /* nobody shoots himself down */
          FBCpa c = ClosestApproach(b0, PrevPose[k], b1, tgt.GetPose(), dt);
          /* The pattern's own size at this point of the flight (dispersion times the path flown), and
           * the gate: three sigma plus the airframe's own reach. Beyond it the density model returns a
           * number no report should carry, and the arithmetic is skipped rather than rounded away. */
          double sigmaM = bundle.Spec->DispersionSigmaRad * bundle.PathM;
          if (sigmaM < 0.05) sigmaM = 0.05;
          if (c.MissM < pass.MinMissM) {
            pass.MinMissM = c.MissM;
            pass.SpreadM = sigmaM;
            pass.PathM = bundle.PathM;
            pass.ClosureMs = c.ClosureMs;
            pass.Rounds = bundle.Rounds;
            pass.TargetName = tgt.GetName();
          }
          if (c.MissM > 3.0 * sigmaM + kGunHitReachM) continue;   /* past the airframe's own reach */
          if (!ResolveGunHit(tgt, c, bundle, sigmaM, c.ClosureMs)) continue;   /* close, but nothing on him */
          Bullets.Retire(bi);   /* the rounds went into him: this bundle is spent */
          pass = FBGunPass{};   /* a hit is its own record; no miss line for this bundle */
          break;
        }
      }
    }

    /* A store's flight ends where its own judge says it does: an impact (the detonation) or the
     * lifetime cap its catalogue entry declares. Retiring is all that happens to the RUN — see
     * FirstFlightKo. In actor order, like every other verdict pass, and AFTER this tick's telemetry
     * sample so the impact tick is the last ROW of the store's trace and not a gap in it. */
    for (auto &t : StoreTracks) {
      FBSimUnit &store = *Actors[t.Index];
      if (!store.Active()) continue;
      FBLogUnitScope us(store.LogLabel());
      /* The proximity fuze first (see ClosestApproach's banner): a round that passed inside its fuze
       * radius during this tick detonated there, whatever it does afterwards. Only for a store that HAS
       * one, only once armed, and only against aircraft — the truth, on the published poses. */
      bool detonated = false;
      if (t.Spec && t.Spec->FuzeRadiusM > 0.0 && HavePrevPose &&
          simT - t.SpawnS >= t.Spec->Perf.ArmingS) {
        for (size_t k = 0; k < Actors.size(); k++) {
          FBSimUnit &tgt = *Actors[k];
          if (k == t.Index || tgt.GetKind() != FBUnitKind::Aircraft || !tgt.Active()) continue;
          FBCpa c = ClosestApproach(PrevPose[t.Index], PrevPose[k], store.GetPose(), tgt.GetPose(), dt);
          if (c.MissM < t.MinMissM && tgt.GetId() != t.LauncherId) {
            t.MinMissM = c.MissM;
            t.MinMissUnit = tgt.GetId();
          }
          if (c.MissM > t.Spec->FuzeRadiusM) continue;
          /* Geometry at the burst, all observed: how far off it was, how fast the two were closing, and
           * from where. What a hit DOES is deliberately not modelled yet — this is the event, not a
           * damage verdict. */
          const fb_fdm_state &ms = store.State();
          double aspect = FBWrap180(tgt.GetPose().YawDeg - ms.yaw);
          FBLog::Info("stores", "DETONATION", {{"target", tgt.GetName()},
              {"missM", c.MissM}, {"fuzeM", t.Spec->FuzeRadiusM}, {"closureMs", c.ClosureMs},
              {"tofS", simT - t.SpawnS + (c.FracT - 1.0) * dt}, {"aspectDeg", aspect}, {"altM", ms.elev},
              {"speedMs", ms.speed}, {"tgtAltM", tgt.GetPose().ElevM},
              {"tgtSpeedMs", tgt.GetPose().SpeedMs}});
          ResolveBurst(tgt, c, *t.Spec);
          store.Retire();
          detonated = true;
          break;
        }
      }
      if (detonated) continue;
      if (store.FlightMonitor().Tripped()) {
        double backS = 0.0;
        fb_fdm_state cross = GroundCrossing(store, backS);
        LogStoreImpact(store, t, simT, cross, backS);
        if (t.MinMissM < 1e17)
          FBLog::Info("stores", "MISS", {{"closestM", t.MinMissM}, {"unitId", t.MinMissUnit},
                                         {"fuzeM", t.Spec ? t.Spec->FuzeRadiusM : 0.0}});
        /* ...and the burst it made, against everything standing near where it landed (see
         * ResolveGroundBurst). Only for a GROUND contact: a store the judge tripped on for tumbling or
         * for a diverged integration did not detonate anywhere in particular. */
        FBKoReason kr = store.FlightMonitor().Reason();
        bool onGround = kr == FBKoReason::StructureContact || kr == FBKoReason::CfitPenetration ||
                        kr == FBKoReason::GearUpContact || kr == FBKoReason::HardLanding ||
                        kr == FBKoReason::AttitudeContact;
        if (onGround && t.Spec && t.Spec->WarheadKg > 0.0) {
          for (auto &g : Actors) {
            if (g->GetKind() != FBUnitKind::Ground || !g->Active()) continue;
            (void)ResolveGroundBurst(*g, cross, *t.Spec);
          }
        }
        store.Retire();
      } else if (simT >= t.DeadlineS) {
        FBLog::Warn("stores", "EXPIRED", {{"tofS", simT - t.SpawnS}, {"altM", store.State().elev},
            {"aglM", store.AglM()}, {"closestM", t.MinMissM < 1e17 ? t.MinMissM : -1.0}});
        store.Retire();
      }
    }
    if (hook) hook->OnTick(Actors, simT);

    /* THE ACTOR LIST'S ONE GROWTH POINT (FBStoreTrack's banner): every store the modules released
     * during this tick becomes a unit now, at the end of it, and is therefore first stepped in the NEXT
     * one. Drained in actor order, each module's queue in FIFO order, so the new actors' order — and
     * with it their ids, their telemetry files and their tick order — is identical no matter how many
     * threads stepped the tick. */
    size_t declaredActors = Actors.size();
    for (size_t i = 0; i < declaredActors; i++) {
      FBSimUnit &carrier = *Actors[i];
      /* THE GUN'S BURSTS, drained in the same pass as the releases and in the same actor order: rounds
       * fired during this tick enter the world now and are first flown in the next one. */
      FBGunBurst gb;
      while (carrier.Module().Guns().TakeBurst(gb)) {
        if (Bullets.Launch(gb)) {
          FBLogUnitScope us(carrier.LogLabel());
          FBLog::Info("gun", "BURST", {{"rounds", gb.Rounds}, {"altM", gb.AltM},
              {"velE", gb.VelE}, {"velN", gb.VelN}, {"velU", gb.VelU},
              {"remaining", carrier.Module().Guns().RoundsRemaining()}});
        } else {
          FBLogUnitScope us(carrier.LogLabel());
          FBLog::Warn("gun", "BURST_DROPPED", {{"rounds", gb.Rounds}, {"live", Bullets.LiveCount()}});
        }
      }
      FBStoreRelease rel;
      while (carrier.Module().Stores().TakeRelease(rel)) {
        const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
        if (!spec) continue;
        char name[64];
        snprintf(name, sizeof name, "%s_%s_%d", carrier.GetName().c_str(), spec->Key,
                 (int)StoreTracks.size() + 1);
        std::string serr;
        std::unique_ptr<FBSimUnit> store =
            FBMissionSpawnStore(models, rel, carrier.State(), carrier.GroundAslM(),
                                (int)Actors.size() + 1, name, carrier.GetTeam(), &serr);
        if (!store) {
          FBLogUnitScope us(carrier.LogLabel());
          FBLog::Error("stores", "SEPARATION_FAILED", {{"station", rel.Station}, {"reason", serr}});
          continue;
        }
        std::string path = TelemetryPath(outDir, Actors.size(), *store);
        ActorTelemetry.emplace_back();
        FBActorTelemetry &tel = ActorTelemetry.back();
        tel.File = FBOpenFile(path.c_str(), "w");
        if (tel.File) {
          tel.Sink = std::make_unique<FBCsvTelemetrySink>(tel.File.get());
          store->StartTelemetry(tel.Sink.get());
        } else {
          store->StartTelemetry(nullptr);   /* it still flies; only its trace is missing */
        }
        StoreTracks.push_back({Actors.size(), simT, simT + spec->MaxFlightS, spec, carrier.GetId(),
                               rel.Solution, 1e18, 0});
        UnitReg.Register(store.get());
        Actors.push_back(std::move(store));
      }
    }

    /* The tick's last act: remember where everything was. One capture point, after the growth above, so
     * every actor in the list — including one that appeared this tick — has an entry from now on. */
    for (size_t i = 0; i < Actors.size(); i++) PrevPose[i] = Actors[i]->GetPose();
    HavePrevPose = true;
  }

  /* ---- Step 4: validate the world — the monitors already did; combine their verdicts ---- */
  const FBSimUnit &primary = *Actors.front();
  const FBSimUnit *ko = FirstFlightKo(Actors);
  /* A physical K.O. always ENDS the run (no wreck integrates in the background) but it only DECIDES it
   * when it was not somebody's declared objective: a shot-down jet that finally reaches the ground is
   * the expected end of that aircraft, not the mission's crash. When it is expected, the run's verdict
   * comes from the mission judges as usual — and the ones still waiting on a `survive` objective are
   * asked here, because the run they had to survive is over. */
  if (ko && ExpectedLoss(Actors, *ko)) {
    FBMissionRoster roster = BuildRoster();
    for (auto &a : Actors) {
      FBLogUnitScope us(a->LogLabel());
      a->FinalizeMission(simT, roster);
    }
    ko = nullptr;
  }
  const FBSimUnit *failed = ko ? nullptr : FirstDecidingFailure(Actors);
  /* The actor whose verdict ENDED the run — a K.O. or the first deciding mission failure. On a clean
   * success nobody decided anything alone (every judged actor met its own objectives), so this stays
   * null and the combined RESULT carries no unit attribution. */
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
