/* FlightBox — FBMissionBoot: the declarative spawn BOTH entry points need to start a mission-with-pilot
 * run (the native --mission runner and the WASM app's default boot), factored once so neither App
 * duplicates JSBSim IC / FBPilot-arming mechanics. Generic over FBModule (mission.ModuleName picks the
 * concrete module via FBModuleRegistry — see FBMissionRunner.cpp), not hardcoded to the F-16: this
 * header itself never names a concrete module type. Header-only: every caller already links
 * FBMissionFile.cpp + the module registry, so no new Makefile translation unit is needed for this.
 *
 * ONE IC application (CLAUDE.md's declarative-spawn contract): FBFdmBoot::Spawn applies position/
 * attitude/velocity together for BOTH a ground sit (HeightOffsetM<0) and an explicit airborne altitude
 * (the offset above the resolved ground) — FBSpawn.Ground picks which, there is no second, separate
 * airborne code path here. This header is also the app-side half of the IC gate: it includes
 * fdm/FBFdmBoot.h, the ONLY way to produce an FBFdm, which is why no file under systems/ or modules/
 * can spawn, re-place or re-trim an airframe (FBFdmBoot.h's banner). */
#ifndef FBMISSIONBOOT_H
#define FBMISSIONBOOT_H

#include <cmath>
#include <string>
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBMissionFile.h"
#include "FBModule.h"
#include "FBFdmBoot.h"
#include <memory>

namespace FlightBox {

/* Applies `mission`'s declarative FBSpawn to `module`/`st`: ONE FBFdmBoot::Spawn IC call (ground-sit or
 * literal-altitude airborne, see the file banner), FBAutopilot neutral Manual (idle stick/throttle — a
 * real-FLCS airframe like the F-16 holds wings-level on its own) so a ground spawn's Preflight hold has
 * something stable to sit in, gear DOWN + both wheel brakes set as the real-world baseline (a mission's
 * own `set gear up` line below overrides it for an air start), the mission's FlightPlan/Runway wired
 * onto the module, FBPilot armed at the phase matching the spawn (Preflight for a ground sit — the
 * existing WOW-gated hold+takeoff-roll machinery; Route directly for an airborne spawn — already
 * established in flight, no taxi/rotate to simulate), then every `set <key> <value>` mission line
 * applied via FBModule::ApplySetup (an unrecognized key fails the spawn — the caller turns that into a
 * mission FAIL, exit 1, per doc/mission-format.md). `aircraftPath` differs per link target (native/gym:
 * "vendor/jsbsim/aircraft", WASM: the embedded FS path "/jsbsim/aircraft"). Returns the spawned,
 * module-attached FBFdm — the CALLER owns it (it outlives the module it flies) — or nullptr (JSBSim
 * init failed, or an unknown `set` key), in which case module/st are not in a usable state. */
inline std::unique_ptr<FBFdm> FBMissionApplySpawn(const char *aircraftPath, const FBMission &mission,
                                                  double groundAsl, FBModule &module, fb_fdm_state &st) {
  constexpr double kKtToMs = 0.51444444444;
  const FBSpawn &sp = mission.Spawn;
  FBFdmSpawn ic;
  ic.ModelsRoot = aircraftPath;
  ic.Aircraft = mission.ModuleName;   /* the module name doubles as the JSBSim model directory */
  ic.LatDeg = sp.LatDeg;
  ic.LonDeg = sp.LonDeg;
  ic.GroundElevM = groundAsl;
  ic.HeightOffsetM = sp.Ground ? -1.0 : (sp.AltM - groundAsl);
  ic.SpeedMs = sp.SpeedKt * kKtToMs;
  ic.HeadingDeg = sp.HeadingDeg;
  std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
  if (!fdm) return nullptr;
  fdm->SetGroundElevM(groundAsl);
  module.AttachFdm(*fdm);   /* before any Controls()/ApplySetup call below reaches the airframe */
  if (mission.HaveRunway) module.SetRunway(mission.Runway);
  module.FlightPlan() = mission.Plan;
  module.Autopilot().SetManual(0.0, 0.0, 0.0, 0.0);
  module.Controls().SetGear(true);
  module.Controls().SetWheelBrakes(1.0, 1.0);
  module.PilotSystem().SetPhase(sp.Ground ? FBPilot::Phase::Preflight : FBPilot::Phase::Route);
  for (const auto &kv : mission.SetKV) {
    if (!module.ApplySetup(kv.first, kv.second)) {
      FBLog::Error("mission", "SET_UNKNOWN_KEY", {{"key", kv.first}, {"value", kv.second}});
      return nullptr;
    }
  }
  st = fb_fdm_state{};
  st.lat = sp.LatDeg; st.lon = sp.LonDeg; st.elev = sp.Ground ? groundAsl : sp.AltM;
  return fdm;
}

/* fb_fdm_state -> core/FBFlightMonitor's narrow, fdm-decoupled input (FBFlightMonitor.h's banner) —
 * header-only so BOTH entry points that own a FBFlightMonitor (FBMissionRunner.cpp for gym/native,
 * FBAppWasm.cpp for the browser) feed it identically without either linking the other's translation
 * unit. */
inline FBFlightMonitorSample FBBuildFlightMonitorSample(const FBFdm &fdm, const fb_fdm_state &st,
                                                        double groundAsl) {
  FBFlightMonitorSample s;
  s.LatDeg = st.lat; s.LonDeg = st.lon;
  s.ElevM = st.elev; s.GroundAslM = groundAsl;
  s.RollDeg = st.roll; s.PitchDeg = st.pitch;
  s.PDegS = st.p; s.QDegS = st.q; s.RDegS = st.r;
  s.VsMs = st.vy; s.TasMs = st.speed;
  s.GearPosNorm = fdm.GetGearPos();
  s.GearForceLbs = fdm.GetMaxGearForceLbs();
  s.WeightLbs = fdm.GetWeightLbs();
  s.AnyWow = fdm.GetWow();
  s.StructureContact = fdm.GetStructureContact();
  return s;
}

} // namespace FlightBox
#endif
