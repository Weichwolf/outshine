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
#include "FBLog.h"
#include "FBMissionFile.h"
#include "FBMissionMonitor.h"
#include "FBModuleRegistry.h"
#include "FBSimUnit.h"
#include "FBUnits.h"
#include "FBFdmBoot.h"

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
 * doc/mission-format.md). An actor WITH objectives (a non-empty flight plan) also gets its own
 * FBMissionMonitor, built from the mission FILE's plan/runway (never the module's live, mutated copy)
 * with `timeoutS` — the caller's resolved timeout, which may override the file's own; an actor the
 * mission gave no waypoints has nothing to succeed or fail at and carries no monitor, so it never
 * appears in the mission verdict.
 *
 * `aircraftPath` differs per link target (native/gym: "vendor/jsbsim/aircraft", WASM: the embedded FS
 * path "/jsbsim/aircraft"). Returns nullptr with a human reason in *err (unknown module, JSBSim init,
 * rejected `set` line); on success the caller owns the actor and everything in it. */
inline std::unique_ptr<FBSimUnit> FBMissionSpawnActor(const char *aircraftPath, const FBMission &mission,
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
  FBFdmSpawn ic;
  ic.ModelsRoot = aircraftPath;
  ic.Aircraft = module->FdmModelName();   /* the MODULE names its JSBSim model; `module <name>` in the
                                           * .fbm stays a pure registry key (FBModule::FdmModelName) */
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

  auto unit = std::make_unique<FBSimUnit>((int)unitIdx + 1, block.Id, block.Team, std::move(fdm),
                                          std::move(module), st, groundAsl);
  if (!block.Plan.Empty())
    unit->SetMissionMonitor(std::make_unique<FBMissionMonitor>(block.Plan, mission.Runway,
                                                               mission.HaveRunway, timeoutS));
  /* THE one place the log-attribution rule is decided (core/FBLog.h's SetUnit banner): a mission with a
   * single actor keeps its lines unattributed — they are the mission's own — while a flight labels
   * every line with the callsign that produced it. */
  unit->SetLogAttribution(mission.Units.size() > 1);
  return unit;
}

} // namespace FlightBox
#endif
