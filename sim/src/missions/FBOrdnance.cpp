#include "FBOrdnance.h"
#include "FBGeodesy.h"
#include "FBGunBallistics.h"
#include "FBLog.h"
#include "FBMissionBoot.h"
#include "FBUnitRegistry.h"
#include "FBUnits.h"
#include <cmath>
#include <cstdio>

namespace FlightBox::Missions {

namespace {
/* Closest approach over the tick SEGMENT, not a per-tick distance test: at 0.1 s and >1500 m/s closure
 * consecutive samples are 150 m apart and a 10 m fuze radius would be missed almost every time.
 * Herleitung: doc/units-and-missions.md §8, "ClosestApproach (CPA)". */
struct FBCpa {
  double MissM = 1e18;
  double ClosureMs = 0.0;
  double FracT = 0.0;   /* where inside the tick, 0..1 — makes the event's time the sub-tick one */
  /* Same vector whose length is MissM: a damage resolution needs the DIRECTION too, and recomputing it
   * elsewhere would be a second, drifting copy of the same geometry. */
  double RelE = 0.0, RelN = 0.0, RelU = 0.0;
};

FBCpa ClosestApproach(const Units::FBUnitPose &a0, const Units::FBUnitPose &b0, const Units::FBUnitPose &a1,
                      const Units::FBUnitPose &b1, double dt) {
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

/* Geometry -> damage, resolved by the OWNER of the simulation on the published poses: letting a weapon
 * score itself on its own seeker estimate would be the purest form of cheating. */
void ResolveBurst(Units::FBSimUnit &target, const FBCpa &c, const FBStoreSpec &spec) {
  const Units::FBUnitPose &p = target.GetPose();
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
  /* One line per changed system, so a damage picture is greppable rather than a bitmask to decode. */
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

/* The three gun gates; Herleitung: doc/units-and-missions.md §8, "ResolveGunHit". */
constexpr double kMinReportedHits = 0.1;
constexpr double kGunHitReachM = 8.0;
constexpr double kGunNearMissM = 200.0;

/* ResolveBurst's kinetic twin: a warhead is a mass the model derives an energy from, a burst is a COUNT
 * of rounds in a pattern, so the energy density is computed here and the damage model handed the result. */
bool ResolveGunHit(Units::FBSimUnit &target, const FBCpa &c, const FBGunProjectiles::Bundle &bundle,
                   double sigmaM, double relSpeedMs) {
  const Units::FBUnitPose &p = target.GetPose();
  double fwd = 0.0, right = 0.0, down = 0.0;
  FBEnuToBodyVec(p.RollDeg, p.PitchDeg, p.YawDeg, c.RelE, c.RelN, c.RelU, fwd, right, down);
  const FBDamageLayout &layout = target.Module().DamageLayout();
  double areaM2 = FBPresentedAreaM2(layout, fwd, right, down);
  if (areaM2 <= 0.0) return false;   /* nothing to hit: a store, a unit with no declared airframe */
  double extentM = FBPresentedExtentM(layout, fwd, right, down);

  double hits = FBGunExpectedHits(bundle.Rounds, c.MissM, sigmaM, areaM2, extentM);
  double flux = FBGunFluxJm2(bundle.Rounds, *bundle.Spec, relSpeedMs, c.MissM, sigmaM, areaM2, extentM);
  if (hits < kMinReportedHits) return false;   /* close, but no rounds on him — the caller records the miss */

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

/* ResolveBurst's unguided counterpart, same 1/r^2 fragment law. Deliberately does NOT reach aircraft:
 * a frag-vs-airframe geometry does not exist here, and an invented cutoff radius would be a number
 * pretending to be physics. Herleitung: doc/units-and-missions.md §8. */
bool ResolveGroundBurst(Units::FBSimUnit &target, const Fdm::fb_fdm_state &burst, const FBStoreSpec &spec) {
  const Units::FBUnitPose &p = target.GetPose();
  double relE = 0.0, relN = 0.0;
  FBEnuOffsetM(p.LatDeg, p.LonDeg, burst.lat, burst.lon, relE, relN);
  double relU = burst.elev - p.ElevM;
  /* The proximity gate is DERIVED, not picked: the lowest threshold this target's own layout declares is
   * the least energy that can do anything to it at all. */
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

/* THE CLUSTER, and the reason it is a different function rather than a flag on the one above: a
 * canister is not a point source but an AREAL ENERGY DENSITY over a declared rectangle, which is
 * exactly the currency FBDamageModel::ApplyKinetic already takes from the gun. Inside the footprint
 * every target takes the same flux, outside it exactly nothing — the one weapon in the tree that turns
 * an aim error from a distance into a binary.
 *
 *   flux = N_sub * (kCaseFraction * m_sub) * 0.5*(kFragSpeedMs^2 + v_impact^2) / (along * across)
 *
 * THE FOOTPRINT IS FIXED PER ROW and NOT a function of release altitude or dispersal speed
 * (doc/air-to-ground.md N2), so where the canister functions makes no difference to what arrives, which
 * is why this is laid at the ground crossing and needs no burst-altitude field. The long axis lies
 * along the canister's own arrival track — a bomblet pattern is stretched by the carrier's motion.
 * Herleitung and the 12 % margin that makes this the softest number in the file: §Knowledge 3. */
bool ResolveClusterBurst(Units::FBSimUnit &target, const Fdm::fb_fdm_state &burst,
                         const FBStoreSpec &spec) {
  const Units::FBUnitPose &p = target.GetPose();
  double alongM = 0.0, acrossM = 0.0;
  FBTrackProjectM(burst.lat, burst.lon, burst.yaw, p.LatDeg, p.LonDeg, alongM, acrossM);
  if (std::fabs(alongM) > 0.5 * spec.FootAlongM || std::fabs(acrossM) > 0.5 * spec.FootAcrossM)
    return false;

  double eSub = kCaseFraction * spec.SubMassKg * 0.5 *
                (kFragSpeedMs * kFragSpeedMs + burst.speed * burst.speed);
  double area = spec.FootAlongM * spec.FootAcrossM;
  FBKineticBurst kb;
  kb.FwdM = 0.0;                       /* the rectangle covers the whole object: no along-axis aiming */
  kb.FluxJm2 = spec.SubCount * eSub / area;
  kb.SpreadM = 0.5 * spec.FootAcrossM;   /* one sigma of the pattern IS the footprint's own half-width */
  kb.Rounds = spec.SubCount;
  kb.ImpactSpeedMs = burst.speed;
  FBDamageResult r = target.TakeKineticBurst(kb);
  FBLogUnitScope us(target.LogLabel());
  FBLog::Info("damage", "CLUSTER", {{"subs", spec.SubCount}, {"fluxJm2", kb.FluxJm2},
      {"alongM", alongM}, {"acrossM", acrossM},
      {"footAlongM", spec.FootAlongM}, {"footAcrossM", spec.FootAcrossM},
      {"impactMs", burst.speed}, {"zone", FBDamageZoneStr(r.Zone)},
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

/* Where the round crossed the surface, as opposed to where it was first SEEN below it: on a 0.1 s tick a
 * Mk-82 penetrates 14 m before it is observed, i.e. ~20 m of horizontal travel — a fifth of the delivery
 * error the attack missions exist to measure. Herleitung: doc/units-and-missions.md §8. */
Fdm::fb_fdm_state GroundCrossing(const Units::FBSimUnit &store, double &backS) {
  Fdm::fb_fdm_state s = store.State();
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
} // namespace

void FBOrdnance::LogStoreImpact(const Units::FBSimUnit &store, const FBStoreTrack &track, double simT,
                                const Fdm::fb_fdm_state &cross, double backS) {
  const Fdm::fb_fdm_state &st = store.State();
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
      {"crossLat", cross.lat}, {"crossLon", cross.lon}, {"crossBackS", backS},
      {"crossTofS", simT - track.SpawnS - backS}});
  /* `predErrM` is what the COMPUTER got wrong (stored table vs. the model's aerodynamics), `aimErrM`
   * what the DELIVERY got wrong. Only for a ground contact: a lost store says nothing about aiming. */
  if (ground && track.Solution.Valid) {
    const FBReleaseSolution &sol = track.Solution;
    double predErr = FBPlanarDistM(sol.ImpactLatDeg, sol.ImpactLonDeg, cross.lat, cross.lon);
    double aimErr = FBPlanarDistM(sol.AimLatDeg, sol.AimLonDeg, cross.lat, cross.lon);
    /* In the ROUND'S arrival direction — a bomb weathercocks into its velocity: + along = it went long. */
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

void FBOrdnance::Reserve(size_t maxActors) {
  Tracks_.reserve(maxActors);
  PrevPose_.assign(maxActors, Units::FBUnitPose{});
}

void FBOrdnance::Resolve(Units::FBActorList &actors, double simT, double dt) {
  /* Fly them, resolve them, and only then (in Launch, with the releases) take on what was fired THIS
   * tick — so a bundle is never resolved in the tick it was created in. */
  Bullets_.Step(dt);
  if (HavePrevPose_) ResolveGuns(actors, dt);
  ResolveStores(actors, simT, dt);
}

void FBOrdnance::ResolveGuns(Units::FBActorList &actors, double dt) {
    for (int bi = 0; bi < Bullets_.Capacity(); bi++) {
      const FBGunProjectiles::Bundle &bundle = Bullets_.At(bi);
      FBGunPass &pass = Passes_[bi];
      if (!bundle.Live) {
        /* The record of how it went is the CLOSEST it ever came, not the first tick it came near. */
        if (pass.Live && pass.MinMissM < kGunNearMissM) {
          FBLog::Info("gun", "MISS", {{"target", pass.TargetName}, {"missM", pass.MinMissM},
              {"spreadM", pass.SpreadM}, {"rounds", pass.Rounds}, {"pathM", pass.PathM},
              {"closureMs", pass.ClosureMs}});
        }
        pass = FBGunPass{};
        continue;
      }
      if (!pass.Live) { pass = FBGunPass{}; pass.Live = true; }
      Units::FBUnitPose b0{}, b1{};
      b0.LatDeg = bundle.PrevLatDeg; b0.LonDeg = bundle.PrevLonDeg; b0.ElevM = bundle.PrevAltM;
      b1.LatDeg = bundle.LatDeg; b1.LonDeg = bundle.LonDeg; b1.ElevM = bundle.AltM;
      for (size_t k = 0; k < actors.size(); k++) {
        Units::FBSimUnit &tgt = *actors[k];
        if (tgt.GetKind() != Units::FBUnitKind::Aircraft || !tgt.Active()) continue;
        if (tgt.GetId() == bundle.LauncherId) continue;   /* nobody shoots himself down */
        FBCpa c = ClosestApproach(b0, PrevPose_[k], b1, tgt.GetPose(), dt);
        /* The pattern's size at this point of the flight: dispersion times the path flown. */
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
        Bullets_.Retire(bi);   /* the rounds went into him: this bundle is spent */
        pass = FBGunPass{};   /* a hit is its own record; no miss line for this bundle */
        break;
      }
    }
}

void FBOrdnance::ResolveStores(Units::FBActorList &actors, double simT, double dt) {
  /* A store's flight ends where its own judge says: an impact or its catalogue lifetime cap. AFTER
   * this tick's telemetry sample, so the impact tick is the last ROW of its trace and not a gap. */
  for (auto &t : Tracks_) {
    Units::FBSimUnit &store = *actors[t.Index];
    if (!store.Active()) continue;
    FBLogUnitScope us(store.LogLabel());
    /* The proximity fuze first: a round that passed inside its radius this tick detonated there,
     * whatever it does afterwards. The arming delay is what keeps a launch off its own launcher. */
    bool detonated = false;
    if (t.Spec && t.Spec->FuzeRadiusM > 0.0 && HavePrevPose_ &&
        simT - t.SpawnS >= t.Spec->Perf.ArmingS) {
      for (size_t k = 0; k < actors.size(); k++) {
        Units::FBSimUnit &tgt = *actors[k];
        /* THE FUZE NOW REACHES THE GROUND TOO, and it had to: doc/weapons.md §5.1 gated on Aircraft
         * alone and §5.3 resolved a store only where it CROSSED the surface, so an air-bursting
         * air-to-ground weapon had no resolution path at all — the boundary was written for the other
         * direction. This is that boundary's mirror image and not its exception: §5.4's refusal (a
         * GROUND burst against an AIRCRAFT, for want of a fragment-against-airframe geometry) is
         * untouched. The LAUNCHER is excluded because a round does not fuze on the rail it left, the
         * same rule the gun path already states. doc/air-to-ground.md §Gaps collision 2. */
        bool reachable = tgt.GetKind() == Units::FBUnitKind::Aircraft ||
                         (tgt.GetKind() == Units::FBUnitKind::Ground && tgt.GetId() != t.LauncherId);
        if (k == t.Index || !reachable || !tgt.Active()) continue;
        FBCpa c = ClosestApproach(PrevPose_[t.Index], PrevPose_[k], store.GetPose(), tgt.GetPose(), dt);
        /* `stores MISS` stays a record about AIRCRAFT and only about them: a surface round passing
         * over a bunker on its way up has not nearly missed it, and letting the ground into this book
         * moved a line in an already-measured file (net-belt-high, measured). Where a round came down
         * against a POSITION is `stores IMPACT`'s crossLat/crossLon, which is the honest place for it. */
        if (c.MissM < t.MinMissM && tgt.GetKind() == Units::FBUnitKind::Aircraft &&
            tgt.GetId() != t.LauncherId) {
          t.MinMissM = c.MissM;
          t.MinMissUnit = tgt.GetId();
        }
        if (c.MissM > t.Spec->FuzeRadiusM) continue;
        const Fdm::fb_fdm_state &ms = store.State();
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
      Fdm::fb_fdm_state cross = GroundCrossing(store, backS);
      LogStoreImpact(store, t, simT, cross, backS);
      if (t.MinMissM < 1e17)
        FBLog::Info("stores", "MISS", {{"closestM", t.MinMissM}, {"unitId", t.MinMissUnit},
                                       {"fuzeM", t.Spec ? t.Spec->FuzeRadiusM : 0.0}});
      /* Only for a GROUND contact: a store the judge tripped on for tumbling or for a diverged
       * integration did not detonate anywhere in particular. */
      FBKoReason kr = store.FlightMonitor().Reason();
      bool onGround = kr == FBKoReason::StructureContact || kr == FBKoReason::CfitPenetration ||
                      kr == FBKoReason::GearUpContact || kr == FBKoReason::HardLanding ||
                      kr == FBKoReason::AttitudeContact;
      if (onGround && t.Spec && (t.Spec->WarheadKg > 0.0 || t.Spec->SubCount > 0)) {
        for (auto &g : actors) {
          if (g->GetKind() != Units::FBUnitKind::Ground || !g->Active()) continue;
          if (t.Spec->SubCount > 0) (void)ResolveClusterBurst(*g, cross, *t.Spec);
          else (void)ResolveGroundBurst(*g, cross, *t.Spec);
        }
      }
      store.Retire();
    } else if (simT >= t.DeadlineS) {
      FBLog::Warn("stores", "EXPIRED", {{"tofS", simT - t.SpawnS}, {"altM", store.State().elev},
          {"aglM", store.AglM()}, {"closestM", t.MinMissM < 1e17 ? t.MinMissM : -1.0}});
      store.Retire();
    }
  }
}

void FBOrdnance::Launch(Units::FBActorList &actors, Units::FBUnitRegistry &units, double simT) {
  /* THE ACTOR LIST'S ONE GROWTH POINT: what was released or fired this tick becomes a unit now, at
   * the end of it, and is first stepped in the NEXT one. Drained in actor order, each module's queue
   * FIFO, so ids/files/tick order are identical no matter how many threads stepped the tick. */
  size_t declared = actors.size();
  for (size_t i = 0; i < declared; i++) {
    Units::FBSimUnit &carrier = *actors[i];
    FBGunBurst gb;
    while (carrier.Module().Guns().TakeBurst(gb)) {
      /* The trigger was pulled — noted before anything can go wrong with the rounds, because what
       * `no_fire` forbids is the pull, not the bundle finding a free slot. */
      carrier.NoteWeaponRelease();
      if (Bullets_.Launch(gb)) {
        FBLogUnitScope us(carrier.LogLabel());
        FBLog::Info("gun", "BURST", {{"rounds", gb.Rounds}, {"altM", gb.AltM},
            {"velE", gb.VelE}, {"velN", gb.VelN}, {"velU", gb.VelU},
            {"remaining", carrier.Module().Guns().RoundsRemaining()}});
      } else {
        FBLogUnitScope us(carrier.LogLabel());
        FBLog::Warn("gun", "BURST_DROPPED", {{"rounds", gb.Rounds}, {"live", Bullets_.LiveCount()}});
      }
    }
    FBStoreRelease rel;
    while (carrier.Module().Stores().TakeRelease(rel)) {
      carrier.NoteWeaponRelease();   /* the store left the rail; a jettison is a release too */
      const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
      if (!spec) continue;
      char name[64];
      snprintf(name, sizeof name, "%s_%s_%d", carrier.GetName().c_str(), spec->Key,
               (int)Tracks_.size() + 1);
      std::string serr;
      std::unique_ptr<Units::FBSimUnit> store =
          FBMissionSpawnStore(Models_, rel, carrier.State(), carrier.GroundAslM(),
                              (int)actors.size() + 1, name, carrier.GetTeam(), &serr);
      if (!store) {
        FBLogUnitScope us(carrier.LogLabel());
        FBLog::Error("stores", "SEPARATION_FAILED", {{"station", rel.Station}, {"reason", serr}});
        continue;
      }
      /* Whatever the owner wants of a store that just became a unit — a CSV in the runner, nothing at
       * all in the browser. It is handed the index it is about to occupy. */
      if (OnSpawn_) OnSpawn_(*store, actors.size());
      Tracks_.push_back({actors.size(), simT, simT + spec->MaxFlightS, spec, carrier.GetId(),
                         rel.Solution, 1e18, 0});
      units.Register(store.get());
      actors.push_back(std::move(store));
    }
  }
}

void FBOrdnance::SnapPoses(const Units::FBActorList &actors) {
  for (size_t i = 0; i < actors.size(); i++) PrevPose_[i] = actors[i]->GetPose();
  HavePrevPose_ = true;
}

} // namespace FlightBox::Missions
