/* FlightBox — FBMissionBoot: the declarative spawn BOTH entry points need to start a mission-with-pilot
 * run (the native/gym mission runner and the WASM app's default boot), factored once so neither App
 * duplicates JSBSim IC / FBPilot-arming mechanics. Generic over FBModule (mission.ModuleName picks the
 * concrete module via FBModuleRegistry), not hardcoded to the F-16: this header itself never names a
 * concrete module type. Header-only: every caller already links FBMissionFile.cpp + the module registry,
 * so no new Makefile translation unit is needed for this.
 *
 * ONE IC application (CLAUDE.md's declarative-spawn contract): FBFdmBoot::Spawn applies position/
 * attitude/velocity together for BOTH a ground sit (HeightOffsetM<0) and an explicit airborne altitude
 * (the offset above the resolved ground) — FBSpawn.Ground picks which, there is no second, separate
 * airborne code path here. This header is also the app-side half of the IC gate: it includes
 * fdm/FBFdmBoot.h, the ONLY way to produce an FBFdm, which is why no file under systems/ or modules/
 * can spawn, re-place or re-trim an airframe (FBFdmBoot.h's banner) — and, since an FBSimUnit can only
 * be built FROM a spawned airframe, why this header is also the only producer of a complete actor. */
#ifndef FBMISSIONBOOT_H
#define FBMISSIONBOOT_H

#include <memory>
#include <string>
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBMissionFile.h"
#include "FBMissionMonitor.h"
#include "FBModuleRegistry.h"
#include "FBSimUnit.h"
#include "FBStore.h"
#include "FBUnits.h"
#include "FBFdmBoot.h"
#include "FBModelRoots.h"

namespace FlightBox {

/* Spawns ONE mission actor — the `unit` block at `unitIdx` (core/FBMissionFile.h): resolves that
 * block's ModuleName through FBModuleRegistry, applies its declarative FBSpawn as a single
 * FBFdmBoot::Spawn IC call (ground-sit or literal-altitude airborne, see the file banner), wires the
 * airframe to the module (FBModule::AttachFdm) and then the block's own data onto it — its FlightPlan
 * and the MISSION's runway, FBAutopilot neutral Manual (idle stick/throttle: a
 * real-FLCS airframe like the F-16 holds wings-level on its own, so a ground spawn's Preflight hold has
 * something stable to sit in), gear DOWN + both wheel brakes as the real-world baseline (a mission's own
 * `set gear up` line overrides it for an air start), FBPilot armed at the phase matching the spawn
 * (Preflight for a ground sit — the WOW-gated hold+takeoff-roll machinery; Route directly for an
 * airborne spawn, already established in flight), and every `set <key> <value>` line via
 * FBModule::ApplySetup (an unrecognized key voids the spawn — the caller turns that into a mission FAIL,
 * doc/mission-format.md). An actor WITH objectives (a non-empty flight plan, `objective` lines, or
 * both) also gets its own FBMissionMonitor, built from the mission FILE's plan/objectives/runway (never
 * the module's live, mutated copy) with `timeoutS` — the caller's resolved timeout, which may override
 * the file's own; an actor the mission gave neither has nothing to succeed or fail at and carries no
 * monitor, so it never appears in the mission verdict.
 *
 * `models` is the client's model root (app/FBModelRoots.h — one root, its spelling differs per link
 * target). Returns nullptr with a human reason in *err (unknown module, JSBSim init,
 * rejected `set` line); on success the caller owns the actor and everything in it. */
inline std::unique_ptr<FBSimUnit> FBMissionSpawnActor(const FBModelRoots &models, const FBMission &mission,
                                                      size_t unitIdx, double groundAsl, double timeoutS,
                                                      std::string *err) {
  auto fail = [err](std::string reason) -> std::unique_ptr<FBSimUnit> {
    if (err) *err = std::move(reason);
    return nullptr;
  };
  const FBMissionUnit &block = mission.Units[unitIdx];
  std::unique_ptr<FBModule> module = FBModuleRegistry::Create(block.ModuleName);
  if (!module) return fail("unknown module '" + block.ModuleName + "'");

  const FBSpawn &sp = block.Spawn;

  /* A MODULE WITH NO AIRFRAME (modules/ground/FBGroundModule — an empty FdmModelName): the whole spawn
   * is the declarative position, and there is nothing to load, place, trim or attach. It is deliberately
   * this early return rather than a branch threaded through the IC below, because the two have nothing
   * in common: everything from here to the end of the aircraft path exists to put a JSBSim instance into
   * a state, and this unit has none. What it DOES share is everything after that — the unit object, its
   * identity, its health register, its telemetry, its mission monitor — which is why those steps are
   * reached the same way in both. */
  if (!module->FdmModelName() || module->FdmModelName()[0] == '\0') {
    if (!sp.Ground) return fail("a unit with no airframe must spawn on the ground");
    if (!block.SetKV.empty()) {
      FBLog::Error("mission", "SET_REJECTED", {{"key", block.SetKV.front().first},
                                               {"value", block.SetKV.front().second}});
      return fail("spawn failed (this module takes no 'set' lines)");
    }
    fb_fdm_state gst{};
    gst.lat = sp.LatDeg; gst.lon = sp.LonDeg; gst.elev = groundAsl; gst.yaw = sp.HeadingDeg;
    FBUnitKind gkind = module->UnitKind();   /* read BEFORE the move: argument order is unspecified */
    auto gunit = std::make_unique<FBSimUnit>((int)unitIdx + 1, block.Id, gkind, block.Team,
                                             nullptr, std::move(module), gst, groundAsl);
    if (!block.Plan.Empty() || !block.Objectives.empty())
      gunit->SetMissionMonitor(std::make_unique<FBMissionMonitor>(block.Plan, block.Objectives,
                                                                  mission.Runway, mission.HaveRunway,
                                                                  timeoutS));
    gunit->SetLogAttribution(mission.Units.size() > 1);
    return gunit;
  }

  FBFdmSpawn ic;
  /* The MODULE names its JSBSim model; `module <name>` in the .fbm stays a pure registry key
   * (FBModule::FdmModelName under the client's one model root, app/FBModelRoots.h). */
  ic.ModelsRoot = models.Aircraft;
  ic.Aircraft = module->FdmModelName();
  ic.LatDeg = sp.LatDeg;
  ic.LonDeg = sp.LonDeg;
  ic.GroundElevM = groundAsl;
  ic.HeightOffsetM = sp.Ground ? -1.0 : (sp.AltM - groundAsl);
  ic.SpeedMs = sp.SpeedKt * kKtToMs;
  ic.HeadingDeg = sp.HeadingDeg;
  std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
  if (!fdm) return fail("spawn failed (jsbsim init, a bad model, or a rejected 'set' line)");
  fdm->SetGroundElevM(groundAsl);
  module->AttachFdm(*fdm);   /* before any Controls()/ApplySetup call below reaches the airframe */
  if (mission.HaveRunway) module->SetRunway(mission.Runway);
  module->FlightPlan() = block.Plan;
  module->Autopilot().SetManual(0.0, 0.0, 0.0, 0.0);
  module->Controls().SetGear(true);
  module->Controls().SetWheelBrakes(1.0, 1.0);
  module->PilotSystem().SetPhase(sp.Ground ? FBPilot::Phase::Preflight : FBPilot::Phase::Route);
  for (const auto &kv : block.SetKV) {
    /* The module already logged WHY (unknown key vs. unparsable/out-of-range value — only it knows its
     * own keys); this is the boot-level "the spawn is void because of this line". */
    if (!module->ApplySetup(kv.first, kv.second)) {
      FBLog::Error("mission", "SET_REJECTED", {{"key", kv.first}, {"value", kv.second}});
      return fail("spawn failed (jsbsim init, a bad model, or a rejected 'set' line)");
    }
  }
  fb_fdm_state st{};
  st.lat = sp.LatDeg; st.lon = sp.LonDeg; st.elev = sp.Ground ? groundAsl : sp.AltM;

  FBUnitKind kind = module->UnitKind();   /* read BEFORE the move: argument order is unspecified */
  auto unit = std::make_unique<FBSimUnit>((int)unitIdx + 1, block.Id, kind, block.Team,
                                          std::move(fdm), std::move(module), st, groundAsl);
  /* An actor is JUDGED iff the mission gave it something to achieve — waypoints to reach or combat
   * objectives to meet (core/FBObjective.h). An actor with neither has nothing to succeed or fail at
   * and carries no monitor, so it never appears in the mission verdict; that was the rule before
   * objectives existed and it is unchanged, only the definition of "something" grew. */
  if (!block.Plan.Empty() || !block.Objectives.empty())
    unit->SetMissionMonitor(std::make_unique<FBMissionMonitor>(block.Plan, block.Objectives,
                                                               mission.Runway, mission.HaveRunway,
                                                               timeoutS));
  /* THE one place the log-attribution rule is decided (core/FBLog.h's SetUnit banner): a mission with a
   * single actor keeps its lines unattributed — they are the mission's own — while a flight labels
   * every line with the callsign that produced it. */
  unit->SetLogAttribution(mission.Units.size() > 1);
  return unit;
}

/* Spawns ONE RELEASED STORE as its own actor: the second producer of a complete unit, and structurally
 * the same four steps as the mission actor above — resolve the module by name (the store's catalogue
 * key IS its registry key, modules/stores/FBStoreModuleRegistration), apply ONE declarative IC, attach
 * the airframe, wrap it in an FBSimUnit. It differs in exactly two things, and both are properties of
 * what a released store IS: the IC comes from the CARRIER's state instead of from a mission file, and
 * the unit is FBUnitKind::Weapon, which is what tells its owner that its physical K.O. is an impact
 * rather than the end of the run.
 *
 * THE SEPARATION STATE, in full:
 *   - POSITION: the carrier's position plus the station's own offset, rotated out of body axes by the
 *     carrier's attitude (core/FBGeodesy's one rotation). A store leaves the pylon, not the CG.
 *   - ATTITUDE: the carrier's, unchanged. Nothing tips it off the rack; whatever the airframe was doing
 *     at that instant is what the store starts doing.
 *   - VELOCITY: the carrier's velocity vector AT THAT STATION, i.e. its CG velocity plus omega x r —
 *     the rotational term matters the moment a release happens in a roll, and leaving it out would be a
 *     silent simplification rather than a modelling choice.
 * There is deliberately NO ejector impulse: a real pylon pushes the store down at some ft/s, and
 * doc/f16/weapons.md has no citable figure for it (§4.5's station data is T4 at best), so the store
 * separates with the carrier's motion and nothing invented on top. When a source turns up it is one
 * added body-axis velocity term here, in this one place.
 *
 * `groundAsl` is the terrain under the release point (the carrier's own current sample — the store is
 * within metres of it). Returns nullptr with a reason in *err. */
inline std::unique_ptr<FBSimUnit> FBMissionSpawnStore(const FBModelRoots &models, const FBStoreRelease &rel,
                                                      const fb_fdm_state &carrier, double groundAsl,
                                                      int unitId, const std::string &name,
                                                      FBUnitTeam team, std::string *err) {
  auto fail = [err](std::string reason) -> std::unique_ptr<FBSimUnit> {
    if (err) *err = std::move(reason);
    return nullptr;
  };
  const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
  if (!spec) return fail("released store is not in the catalogue");
  std::unique_ptr<FBModule> module = FBModuleRegistry::Create(spec->Key);
  if (!module) return fail(std::string("no module registered for store '") + spec->Key + "'");

  double offE = 0.0, offN = 0.0, offU = 0.0;
  FBBodyVecToEnu(carrier.roll, carrier.pitch, carrier.yaw, rel.OffFwdM, rel.OffRightM, rel.OffDownM,
                 offE, offN, offU);
  /* omega x r in body axes (rates deg/s -> rad/s), then the same rotation into ENU. */
  double p = carrier.p * kDeg2Rad, q = carrier.q * kDeg2Rad, r = carrier.r * kDeg2Rad;
  double rotFwd = q * rel.OffDownM - r * rel.OffRightM;
  double rotRight = r * rel.OffFwdM - p * rel.OffDownM;
  double rotDown = p * rel.OffRightM - q * rel.OffFwdM;
  double rotE = 0.0, rotN = 0.0, rotU = 0.0;
  FBBodyVecToEnu(carrier.roll, carrier.pitch, carrier.yaw, rotFwd, rotRight, rotDown, rotE, rotN, rotU);

  double coslat = std::cos(carrier.lat * kDeg2Rad);
  FBFdmSpawn ic;
  ic.ModelsRoot = models.Aircraft;
  ic.Aircraft = module->FdmModelName();
  ic.LatDeg = carrier.lat + offN / kMPerDeg;
  ic.LonDeg = carrier.lon + (coslat > 1e-6 ? offE / (kMPerDeg * coslat) : 0.0);
  ic.GroundElevM = groundAsl;
  double aglM = carrier.elev + offU - groundAsl;
  ic.HeightOffsetM = aglM > 0.5 ? aglM : 0.5;   /* the IC needs a positive offset; below this it is
                                                 * about to hit the ground anyway */
  ic.HeadingDeg = carrier.yaw;
  ic.Ballistic = true;
  ic.PitchDeg = carrier.pitch;
  ic.RollDeg = carrier.roll;
  /* fb_fdm_state velocity is X-Plane local (+x east, +y up, +z south) — see fdm/FBFdm.h. */
  ic.VelNorthMs = -carrier.vz + rotN;
  ic.VelEastMs = carrier.vx + rotE;
  ic.VelDownMs = -carrier.vy - rotU;

  std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
  if (!fdm) return fail(std::string("store spawn failed (jsbsim init or a bad model: ") + ic.Aircraft + ")");
  fdm->SetGroundElevM(groundAsl);
  module->AttachFdm(*fdm);
  /* The launch programming, generically (FBModule::ProgramRelease): a bomb ignores it, a guided round
   * takes its launcher's id and its target estimate from it. This file names no weapon type. */
  module->ProgramRelease(rel);

  fb_fdm_state st{};
  st.lat = ic.LatDeg; st.lon = ic.LonDeg; st.elev = groundAsl + ic.HeightOffsetM;
  st.roll = ic.RollDeg; st.pitch = ic.PitchDeg; st.yaw = ic.HeadingDeg;
  auto unit = std::make_unique<FBSimUnit>(unitId, name, FBUnitKind::Weapon, team, std::move(fdm),
                                          std::move(module), st, groundAsl);
  /* A store never flies alone — there is always at least the jet that dropped it — so its lines are
   * always attributed to its own callsign. */
  unit->SetLogAttribution(true);
  FBLog::Info("stores", "SEPARATION", {{"store", spec->Key}, {"station", rel.Station},
      {"lat", ic.LatDeg}, {"lon", ic.LonDeg}, {"altM", st.elev}, {"aglM", ic.HeightOffsetM},
      {"vNorthMs", ic.VelNorthMs}, {"vEastMs", ic.VelEastMs}, {"vDownMs", ic.VelDownMs},
      {"pitchDeg", ic.PitchDeg}, {"rollDeg", ic.RollDeg}, {"massLbs", rel.MassLbs}});
  return unit;
}

} // namespace FlightBox
#endif
