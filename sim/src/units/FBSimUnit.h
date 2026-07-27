/* FlightBox — FBSimUnit: ONE simulated unit, whole. Everything that used to be a scattered per-run
 * local in the mission runner (an FBFdm, a module from the registry, the shared fb_fdm_state, the
 * resolved ground ASL, the telemetry source/bus, the two judges) and a parallel set of file-scope
 * statics in the browser client is HERE, as one object with one owner. A client holds a LIST of these
 * — one per `unit` block the mission declares (core/FBMissionFile.h), stepped in list order.
 *
 * It IS an FBUnit — the world-entity identity (Id/Name/Kind/Team) and Pose the FBWorld registry hands to
 * sensors/weapons — PUBLISHED once per tick from the live state (FBUnit's snapshot contract: cross-unit
 * reads see the last completed tick, never a half-integrated one), which is the one place a copy of the
 * truth is legitimate. There is deliberately no separate "ownship view" unit type beside it: one
 * concept, so nothing above this layer has to ask which kind of unit it is looking at.
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
#include <string>
#include <vector>
#include "FBDamageModel.h"
#include "FBFdm.h"
#include "FBFdmTelemetrySource.h"
#include "FBFlightMonitor.h"
#include "FBMissionMonitor.h"
#include "FBModule.h"
#include "FBState.h"
#include "FBStateBusTelemetry.h"
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
  FBSimUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team, std::unique_ptr<FBFdm> fdm,
            std::unique_ptr<FBModule> module, const fb_fdm_state &initialState, double groundAslM);

  /* ---- FBUnit ---- */
  /* The PUBLISHED pose (FBUnit's snapshot contract): what every other unit and every client sees is the
   * last completed tick, made visible by PublishPose() after ALL units have run. */
  FBUnitPose GetPose() const override { return Pose_; }
  /* Published together with the pose, at the same barrier: what this unit RADIATES (FBUnit.h's
   * FBUnitSignature) — today its datalink transmitter, read by every OTHER unit's terminal. */
  FBUnitSignature GetSignature() const override { return Sig_; }
  /* The module cycles its own FDM + systems; `units` reaches only its sensor slots. */
  void Run(double dt, const FBUnitRegistry *units, const FBWorld *world) override;

  /* The tick barrier: copies this unit's freshly integrated state into the published pose + signature.
   * A client calls it for EVERY unit only once every unit has run — that ordering, not this function, is what
   * makes a multi-unit tick order-independent. */
  void PublishPose();

  /* This unit's FBLog attribution (core/FBLogUnitScope) — empty when the mission has a single actor,
   * whose lines need no disambiguation. Set once at boot (app/FBMissionBoot.h), never per tick. */
  void SetLogAttribution(bool on) { LogLabel_ = on ? GetName() : std::string(); }
  const std::string &LogLabel() const { return LogLabel_; }

  /* ---- lifetime ----
   * A unit is ACTIVE until its owner retires it. Retiring stops the stepping and the telemetry, and
   * nothing else: the object stays alive for the rest of the run, because the unit registry borrows
   * raw pointers and everything that has already been written about it (its telemetry file, its lines
   * in events.log) must stay valid. It is how a released store leaves the simulation once it has hit
   * the ground — the trajectory ends, the record does not. Deliberately NOT erasure from the actor
   * list: that would move every later actor's index and make a run's tick order depend on when a bomb
   * happened to land. */
  bool Active() const { return Active_; }
  /* Retiring also SILENCES the unit: its published emission signature goes empty in the same act. A
   * detonated round's seeker was still radiating in its last snapshot, and because that snapshot is
   * frozen from then on, everything with a warning receiver would have gone on hearing a missile that no
   * longer exists (measured: the shooter's own RWR reported a live seeker two minutes after the
   * detonation). The pose stays — it is where the unit ended up, and the record must stay valid — but
   * nothing that has left the simulation may keep emitting into it. */
  void Retire() { Active_ = false; Sig_ = FBUnitSignature{}; }

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
  void PrimeState() { Fdm_->Step(St_); PublishPose(); }

  /* ---- battle damage ----
   * The unit owns its health register (core/FBSystemHealth) for the same reason it owns its two judges:
   * it is a fact ABOUT the unit that the unit's own module may read but must never write. The module
   * gets a const handle at construction (FBModule::AttachHealth); the only thing that can change a state
   * is a resolved burst, and this is where one arrives.
   *
   * TakeBurst is the whole consequence chain in one call: core/FBDamageModel decides what the geometry
   * did to which system (the module's own layout says which systems are where), and the outcome is
   * pushed straight into the airframe — engine cutoff, control authority, drag — so that from the next
   * step onwards JSBSim is flying the damaged aircraft. Nothing is "marked dead": the unit keeps being
   * stepped and keeps being judged by the same two monitors as before. */
  FBDamageResult TakeBurst(const FBBurst &burst);
  /* ...and the same chain for a burst of gunfire (core/FBDamageModel's FBKineticBurst): a different
   * kind of arriving energy, the same register, the same push into the airframe afterwards. */
  FBDamageResult TakeKineticBurst(const FBKineticBurst &burst);
  const FBSystemHealth &Health() const { return Health_; }

  /* ---- the two incorruptible judges (fed here, never handed to the module) ---- */
  void SetMissionMonitor(std::unique_ptr<FBMissionMonitor> monitor) { Mission_ = std::move(monitor); }
  const FBFlightMonitor &FlightMonitor() const { return Flight_; }
  const FBMissionMonitor *MissionMonitor() const { return Mission_.get(); }   /* null: no objectives */

  /* Feeds both judges this tick's observed truth and applies the ONE consequence a trip has on the
   * airframe (engine cutoff — JSBSim's own ground reactions do the rest, no freeze). Returns true if
   * either concluded on this tick. `roster` is the observed state of the OTHER units (core/
   * FBObjective.h) that a combat objective is judged against — built by the caller from the health
   * registers IT owns, empty for a mission that declares none. */
  bool RunMonitors(double simT, const FBMissionRoster &roster = FBMissionRoster{});

  /* The run is over and this unit's mission judge has not concluded — ask it (FBMissionMonitor::
   * Finalize; a `survive` objective can only be answered here). A no-op for an already-concluded or
   * absent monitor, which is every legacy one. */
  bool FinalizeMission(double simT, const FBMissionRoster &roster);

  /* Generic flight-envelope diagnostics (stall/overspeed/sink) — every module has these quantities, so
   * they are the unit's own observation, latched per unit rather than per run. */
  void CheckEnvelope();

  /* ---- telemetry ---- */
  /* Registers this unit's sources in fixed column order (FDM pose, air data, pilot, FCS, controls) and
   * starts the bus. A null sink leaves the bus a cheap no-op (the browser's case). */
  void StartTelemetry(FBTelemetrySink *sink);
  void SampleTelemetry(double simT) { Bus_.Tick(simT); }

private:
  void ApplyDamageToAirframe();   /* health register -> JSBSim, the only place damage becomes physics */
  FBMissionMonitorSample BuildMissionSample(const FBMissionRoster &roster) const;

  std::unique_ptr<FBFdm> Fdm_;         /* owned; outlives Module_, which only borrows it */
  std::unique_ptr<FBModule> Module_;
  fb_fdm_state St_;
  FBUnitPose Pose_;                    /* the published (last completed tick) pose — see GetPose() */
  FBUnitSignature Sig_;                /* published with it — see GetSignature() */
  std::string LogLabel_;
  double GroundAslM_;
  FBSystemHealth Health_;              /* written only by core/FBDamageModel — see TakeBurst */
  FBFdmTelemetrySource FdmSrc_;        /* borrows Fdm_/St_/GroundAslM_ — all declared above it */
  FBStateBusTelemetry BusSrc_;         /* borrows the module's own state bus (validity per block) */
  FBSystemHealthTelemetry HealthSrc_;  /* borrows Health_, declared above it */
  FBTelemetryBus Bus_;
  FBFlightMonitor Flight_;
  std::unique_ptr<FBMissionMonitor> Mission_;   /* absent for a unit with no mission to judge */
  bool WarnedStall_ = false, WarnedOverspeed_ = false, WarnedSink_ = false;
  bool Active_ = true;
};

/* A mission's cast, in declaration order — one entry per `unit` block (core/FBMissionFile.h). Index 0
 * is the PRIMARY actor: its telemetry keeps the canonical file name and its eye is the camera's. The
 * order is also the fixed tick order, which together with the pose barrier (GetPose's contract) is what
 * makes a multi-actor run reproducible. Every client that runs a sim loop owns one of these. */
using FBActorList = std::vector<std::unique_ptr<FBSimUnit>>;

} // namespace FlightBox
#endif
