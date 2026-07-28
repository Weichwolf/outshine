/* The declarative spawn both entry points share (the mission runner and the WASM boot), generic over
 * FBModule — this header never names a concrete module type. ONE IC application covers ground sit and
 * airborne alike; there is no second code path.
 *
 * It is also the app-side half of the IC gate: it includes fdm/FBFdmBoot.h, the only way to produce an
 * FBFdm — which is why nothing under systems/ or modules/ can spawn or re-trim an airframe, and why
 * this header is the only producer of a complete actor. Ablauf: doc/units-and-missions.md §6. */
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

namespace FlightBox::Missions {

/* Spawns ONE mission actor: the `unit` block at `unitIdx`, step by step in
 * doc/units-and-missions.md §6. Returns nullptr with a human reason in *err; on success the
 * caller owns the actor and everything in it. */
inline std::unique_ptr<Units::FBSimUnit> FBMissionSpawnActor(const FBModelRoots &models, const FBMission &mission,
                                                      size_t unitIdx, double groundAsl, double timeoutS,
                                                      std::string *err) {
  auto fail = [err](std::string reason) -> std::unique_ptr<Units::FBSimUnit> {
    if (err) *err = std::move(reason);
    return nullptr;
  };
  const FBMissionUnit &block = mission.Units[unitIdx];
  std::unique_ptr<Modules::FBModule> module = Modules::FBModuleRegistry::Create(block.ModuleName);
  if (!module) return fail("unknown module '" + block.ModuleName + "'");

  const FBSpawn &sp = block.Spawn;

  /* A MODULE WITH NO AIRFRAME (an empty FdmModelName): deliberately an early return rather than a
   * branch threaded through the IC, because everything below exists to put a JSBSim instance into a
   * state and this unit has none. Everything AFTER that is shared. */
  if (!module->FdmModelName() || module->FdmModelName()[0] == '\0') {
    if (!sp.Ground) return fail("a unit with no airframe must spawn on the ground");
    if (!block.SetKV.empty()) {
      FBLog::Error("mission", "SET_REJECTED", {{"key", block.SetKV.front().first},
                                               {"value", block.SetKV.front().second}});
      return fail("spawn failed (this module takes no 'set' lines)");
    }
    Fdm::fb_fdm_state gst{};
    gst.lat = sp.LatDeg; gst.lon = sp.LonDeg; gst.elev = groundAsl; gst.yaw = sp.HeadingDeg;
    Units::FBUnitKind gkind = module->UnitKind();   /* read BEFORE the move: argument order is unspecified */
    auto gunit = std::make_unique<Units::FBSimUnit>((int)unitIdx + 1, block.Id, gkind, block.Team,
                                             nullptr, std::move(module), gst, groundAsl, block.Flight);
    if (!block.Plan.Empty() || !block.Objectives.empty())
      gunit->SetMissionMonitor(std::make_unique<FBMissionMonitor>(block.Plan, block.Objectives,
                                                                  mission.Runway, mission.HaveRunway,
                                                                  timeoutS));
    gunit->SetLogAttribution(mission.Units.size() > 1);
    return gunit;
  }

  Fdm::FBFdmSpawn ic;
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
  std::unique_ptr<Fdm::FBFdm> fdm = Fdm::FBFdmBoot::Spawn(ic);
  if (!fdm) return fail("spawn failed (jsbsim init, a bad model, or a rejected 'set' line)");
  fdm->SetGroundElevM(groundAsl);
  module->AttachFdm(*fdm);   /* before any Controls()/ApplySetup call below reaches the airframe */
  if (mission.HaveRunway) module->SetRunway(mission.Runway);
  module->FlightPlan() = block.Plan;
  module->Autopilot().SetManual(0.0, 0.0, 0.0, 0.0);
  module->Controls().SetGear(true);
  module->Controls().SetWheelBrakes(1.0, 1.0);
  module->PilotSystem().SetPhase(sp.Ground ? Pilot::FBPilot::Phase::Preflight : Pilot::FBPilot::Phase::Route);
  for (const auto &kv : block.SetKV) {
    /* The module already logged WHY — only it knows its keys; this is "the spawn is void". */
    if (!module->ApplySetup(kv.first, kv.second)) {
      FBLog::Error("mission", "SET_REJECTED", {{"key", kv.first}, {"value", kv.second}});
      return fail("spawn failed (jsbsim init, a bad model, or a rejected 'set' line)");
    }
  }
  Fdm::fb_fdm_state st{};
  st.lat = sp.LatDeg; st.lon = sp.LonDeg; st.elev = sp.Ground ? groundAsl : sp.AltM;

  Units::FBUnitKind kind = module->UnitKind();   /* read BEFORE the move: argument order is unspecified */
  auto unit = std::make_unique<Units::FBSimUnit>((int)unitIdx + 1, block.Id, kind, block.Team,
                                          std::move(fdm), std::move(module), st, groundAsl, block.Flight);
  /* An actor is JUDGED iff the mission gave it something to achieve; one with neither waypoints nor
   * objectives carries no monitor and never appears in the mission verdict. */
  if (!block.Plan.Empty() || !block.Objectives.empty())
    unit->SetMissionMonitor(std::make_unique<FBMissionMonitor>(block.Plan, block.Objectives,
                                                               mission.Runway, mission.HaveRunway,
                                                               timeoutS));
  /* THE one place the log-attribution rule is decided: a single actor's lines stay unattributed
   * (they are the mission's own), a flight labels every line with its callsign. */
  unit->SetLogAttribution(mission.Units.size() > 1);
  return unit;
}

/* The second producer of a complete unit: structurally the same four steps as the mission actor, but
 * the IC comes from the CARRIER's state rather than a mission file, and the unit is a Weapon.
 * There is deliberately NO ejector impulse — no citable figure exists, so the store separates with the
 * carrier's motion and nothing invented on top; a source would add ONE body-axis term, here.
 * Der vollstaendige Separationszustand: doc/units-and-missions.md §6. */
inline std::unique_ptr<Units::FBSimUnit> FBMissionSpawnStore(const FBModelRoots &models, const FBStoreRelease &rel,
                                                      const Fdm::fb_fdm_state &carrier, double groundAsl,
                                                      int unitId, const std::string &name,
                                                      FBUnitTeam team, std::string *err) {
  auto fail = [err](std::string reason) -> std::unique_ptr<Units::FBSimUnit> {
    if (err) *err = std::move(reason);
    return nullptr;
  };
  const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
  if (!spec) return fail("released store is not in the catalogue");
  std::unique_ptr<Modules::FBModule> module = Modules::FBModuleRegistry::Create(spec->Key);
  if (!module) return fail(std::string("no module registered for store '") + spec->Key + "'");

  double offE = 0.0, offN = 0.0, offU = 0.0;
  FBBodyVecToEnu(carrier.roll, carrier.pitch, carrier.yaw, rel.OffFwdM, rel.OffRightM, rel.OffDownM,
                 offE, offN, offU);
  /* omega x r in body axes, then the same rotation into ENU: the term that matters in a roll. */
  double p = carrier.p * kDeg2Rad, q = carrier.q * kDeg2Rad, r = carrier.r * kDeg2Rad;
  double rotFwd = q * rel.OffDownM - r * rel.OffRightM;
  double rotRight = r * rel.OffFwdM - p * rel.OffDownM;
  double rotDown = p * rel.OffRightM - q * rel.OffFwdM;
  double rotE = 0.0, rotN = 0.0, rotU = 0.0;
  FBBodyVecToEnu(carrier.roll, carrier.pitch, carrier.yaw, rotFwd, rotRight, rotDown, rotE, rotN, rotU);

  double coslat = std::cos(carrier.lat * kDeg2Rad);
  Fdm::FBFdmSpawn ic;
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

  std::unique_ptr<Fdm::FBFdm> fdm = Fdm::FBFdmBoot::Spawn(ic);
  if (!fdm) return fail(std::string("store spawn failed (jsbsim init or a bad model: ") + ic.Aircraft + ")");
  fdm->SetGroundElevM(groundAsl);
  module->AttachFdm(*fdm);
  /* Generic launch programming: a bomb ignores it, a guided round takes its launcher id and target
   * estimate from it. This file names no weapon type. */
  module->ProgramRelease(rel);

  Fdm::fb_fdm_state st{};
  st.lat = ic.LatDeg; st.lon = ic.LonDeg; st.elev = groundAsl + ic.HeightOffsetM;
  st.roll = ic.RollDeg; st.pitch = ic.PitchDeg; st.yaw = ic.HeadingDeg;
  auto unit = std::make_unique<Units::FBSimUnit>(unitId, name, Units::FBUnitKind::Weapon, team, std::move(fdm),
                                          std::move(module), st, groundAsl);
  /* A store never flies alone, so its lines are always attributed. */
  unit->SetLogAttribution(true);
  FBLog::Info("stores", "SEPARATION", {{"store", spec->Key}, {"station", rel.Station},
      {"lat", ic.LatDeg}, {"lon", ic.LonDeg}, {"altM", st.elev}, {"aglM", ic.HeightOffsetM},
      {"vNorthMs", ic.VelNorthMs}, {"vEastMs", ic.VelEastMs}, {"vDownMs", ic.VelDownMs},
      {"pitchDeg", ic.PitchDeg}, {"rollDeg", ic.RollDeg}, {"massLbs", rel.MassLbs}});
  return unit;
}

} // namespace FlightBox::Missions
#endif
