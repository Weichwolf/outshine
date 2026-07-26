/* FlightBox — FBSimUnit: ONE simulated unit, whole. Everything that used to be a scattered per-run
 * local in the mission runner (an FBFdm, a module from the registry, the shared fb_fdm_state, the
 * resolved ground ASL, the telemetry source/bus, the two judges) and a parallel set of file-scope
 * statics in the browser client is HERE, as one object with one owner. A client holds a LIST of these;
 * today every list has exactly one element, which is a property of today's mission file (one actor
 * block), not of this type.
 *
 * It IS an FBUnit — the world-entity identity (Id/Kind/Team) and Pose the FBWorld registry hands to
 * sensors/weapons, derived on read from the live state (never a shadow copy that can drift). There is
 * deliberately no separate "ownship view" unit type beside it: one concept, so nothing above this layer
 * has to ask which kind of unit it is looking at.
 *
 * OWNERSHIP: the unit owns its airframe (unique_ptr<FBFdm>) and the module that flies it
 * (unique_ptr<FBModule>) — declared in that order so the airframe outlives the module that borrows it.
 * The state POD, ground truth, telemetry source and both monitors are plain members. Everything the
 * unit hands out is borrowed (`const&`/`*`), and the telemetry SINK stays with the client (file I/O is
 * app/'s, core/ stays I/O-free).
 *
 * ANTI-CHEAT (CLAUDE.md "Kein Cheaten") is unweakened by the bundling: an FBSimUnit can only be
 * constructed from an already-spawned FBFdm, and the only producer of one is fdm/FBFdmBoot (app/-only),
 * so nothing under systems/ or modules/ can build a unit, reach a monitor, or re-place an airframe —
 * `grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems src/modules` is empty and stays
 * empty. The module still never sees the judges: they are fed HERE, from observed FDM truth, and the
 * only thing a trip does to the airframe is the same engine cutoff the App always applied. */
#ifndef FBSIMUNIT_H
#define FBSIMUNIT_H

#include <memory>
#include "FBFdm.h"
#include "FBFdmTelemetrySource.h"
#include "FBFlightMonitor.h"
#include "FBMissionMonitor.h"
#include "FBModule.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBUnit.h"

namespace FlightBox {

/* fb_fdm_state -> core/FBFlightMonitor's narrow, fdm-decoupled input (FBFlightMonitor.h's banner).
 * Free + inline because the dedicated monitor test harnesses (app/FBTest*.cpp) are miniature units:
 * they drive an FBFdm by hand and feed the same judge, and must build the sample the same way a real
 * unit does — one definition, no second, drifting copy of what the monitor is shown. */
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
  s.FdmFault = fdm.Faulted();
  return s;
}

class FBSimUnit : public FBUnit {
public:
  /* `fdm` must be a spawned, trimmed airframe (fdm/FBFdmBoot::Spawn) and `module` an already
   * AttachFdm'd module flying it — the boot sequence that produces both lives in app/FBMissionBoot.h,
   * the one place allowed to name the IC header. `initialState` is the state that boot left behind
   * (the declarative spawn position, before the first step). */
  FBSimUnit(int id, FBUnitTeam team, std::unique_ptr<FBFdm> fdm, std::unique_ptr<FBModule> module,
            const fb_fdm_state &initialState, double groundAslM);

  /* ---- FBUnit ---- */
  FBUnitPose GetPose() const override;
  void Run(double dt, const FBWorld *world) override;   /* the module cycles its own FDM + systems */

  /* ---- state + ground truth ---- */
  const fb_fdm_state &State() const { return St_; }
  const FBFdm &Fdm() const { return *Fdm_; }              /* read-only handle, see FBFdm.h */
  FBModule &Module() { return *Module_; }
  const FBModule &Module() const { return *Module_; }
  /* The module's Displays slot as the renderer wants it (FBRenderer::SetHudDisplay takes a const
   * pointer): FBModule's accessors are non-const by design — systems are COMMANDED through them — so
   * the read-only view a display consumer needs is surfaced here instead of const_cast at call sites. */
  const FBDisplaySystem &Displays() const { return Module_->Displays(); }

  /* This tick's ground elevation sample (m ASL) from the client's FBElevationProvider. An unresolved
   * sample keeps the last good value: ONE number reaches both JSBSim's contact floor and the module's
   * HUD/radar-alt path, so the two can never disagree about where the ground is. */
  void UpdateGroundAsl(double sampleM);
  double GroundAslM() const { return GroundAslM_; }
  double AglM() const { return St_.elev - GroundAslM_; }

  /* The module's HUD-ready FBState with this tick's live pose folded in — the fill both renderer
   * clients did line-for-line identically before each adding its own extras (home vector/mode in the
   * browser, a fixed mode in the native oracle). */
  FBState HudState() const;

  /* One FDM step to fill the shared state before the first frame reads pose/HUD (boot only; the module
   * drives every step after that). */
  void PrimeState() { Fdm_->Step(St_); }

  /* ---- the two incorruptible judges (fed here, never handed to the module) ---- */
  void SetMissionMonitor(std::unique_ptr<FBMissionMonitor> monitor) { Mission_ = std::move(monitor); }
  const FBFlightMonitor &FlightMonitor() const { return Flight_; }
  const FBMissionMonitor *MissionMonitor() const { return Mission_.get(); }
  bool MissionConcluded() const { return Mission_ && Mission_->Concluded(); }

  /* Feeds both judges this tick's observed truth and applies the ONE consequence a trip has on the
   * airframe (engine cutoff — JSBSim's own ground reactions do the rest, no freeze). Returns true if
   * either concluded on this tick. */
  bool RunMonitors(double simT);

  /* Generic flight-envelope diagnostics (stall/overspeed/sink) — every module has these quantities, so
   * they are the unit's own observation, latched per unit rather than per run. */
  void CheckEnvelope();

  /* ---- telemetry ---- */
  /* Registers this unit's sources in fixed column order (FDM pose, air data, pilot, FCS, controls) and
   * starts the bus. A null sink leaves the bus a cheap no-op (the browser's case). */
  void StartTelemetry(FBTelemetrySink *sink);
  void SampleTelemetry(double simT) { Bus_.Tick(simT); }

private:
  std::unique_ptr<FBFdm> Fdm_;         /* owned; outlives Module_, which only borrows it */
  std::unique_ptr<FBModule> Module_;
  fb_fdm_state St_;
  double GroundAslM_;
  FBFdmTelemetrySource FdmSrc_;        /* borrows Fdm_/St_/GroundAslM_ — all declared above it */
  FBTelemetryBus Bus_;
  FBFlightMonitor Flight_;
  std::unique_ptr<FBMissionMonitor> Mission_;   /* absent for a unit with no mission to judge */
  bool WarnedStall_ = false, WarnedOverspeed_ = false, WarnedSink_ = false;
};

} // namespace FlightBox
#endif
