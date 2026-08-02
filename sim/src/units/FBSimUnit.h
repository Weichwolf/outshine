/* FlightBox — FBSimUnit: ONE simulated unit, whole — airframe, module, state, ground truth, telemetry
 * and both incorruptible judges, one object with one owner. A client holds a LIST of these, one per
 * `unit` block the mission declares. THE AIRFRAME IS OPTIONAL (a static ground target has no flight
 * dynamics); everything else here is universal. Details: doc/units-and-missions.md */
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
#include "FBWeatherProvider.h"
#include "FBTelemetry.h"
#include "FBUnit.h"

namespace FlightBox::Missions { class FBMissionSim; }   /* the ONE driver of a tick — see below */

namespace FlightBox::Units {

/* Free + inline because the monitor test harnesses are miniature units: they must build the sample the
 * same way a real one does, so there is no second, drifting copy of what the judge is shown. */
inline FBFlightMonitorSample FBBuildFlightMonitorSample(const Fdm::FBFdm &fdm, const Fdm::fb_fdm_state &st,
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
  /* `fdm` must be spawned and trimmed (fdm/FBFdmBoot::Spawn), `module` already AttachFdm'd; a NULL
   * `fdm` means exactly one thing, that this unit has no flight dynamics. */
  FBSimUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team, std::unique_ptr<Fdm::FBFdm> fdm,
            std::unique_ptr<Modules::FBModule> module, const Fdm::fb_fdm_state &initialState, double groundAslM,
            FBFlightId flight = FBFlightId{});

  /* ---- FBUnit ---- */
  FBUnitPose GetPose() const override { return Pose_; }
  FBUnitSignature GetSignature() const override { return Sig_; }

  /* FBLog attribution, empty when a single actor needs no disambiguation. Set once at boot. */
  void SetLogAttribution(bool on) { LogLabel_ = on ? GetName() : std::string(); }
  const std::string &LogLabel() const { return LogLabel_; }

  /* ---- lifetime ----
   * Retiring stops stepping and telemetry and nothing else: the object stays alive because the registry
   * borrows raw pointers and everything already written about it must stay valid. Deliberately NOT
   * erasure — that would move every later index and tie tick order to when a bomb happened to land. */
  bool Active() const { return Active_; }
  /* Retiring also SILENCES the unit: otherwise its last snapshot keeps radiating forever, and every
   * warning receiver goes on hearing a missile that no longer exists (measured). */
  void Retire() { Active_ = false; Sig_ = FBUnitSignature{}; }

  /* ---- state + ground truth ---- */
  const Fdm::fb_fdm_state &State() const { return St_; }
  const Fdm::FBFdm *Fdm() const { return Fdm_.get(); }   /* read-only handle; null = no airframe */
  Modules::FBModule &Module() { return *Module_; }
  const Modules::FBModule &Module() const { return *Module_; }
  /* FBModule's accessors are non-const by design (systems are COMMANDED through them), so the read-only
   * view a display consumer needs is surfaced here instead of const_cast at call sites. */
  const Systems::FBDisplaySystem &Displays() const { return Module_->Displays(); }

  double GroundAslM() const { return GroundAslM_; }
  double AglM() const { return St_.elev - GroundAslM_; }

  FBState HudState() const;

  /* ---- battle damage ----
   * The whole consequence chain in one call: core/FBDamageModel decides what the geometry did to which
   * system, and the outcome is pushed straight into the airframe, so from the next step onwards JSBSim
   * is flying the damaged aircraft. Nothing is "marked dead" — the unit keeps being stepped and judged. */
  FBDamageResult TakeBurst(const FBBurst &burst);
  FBDamageResult TakeKineticBurst(const FBKineticBurst &burst);
  const FBSystemHealth &Health() const { return Health_; }

  /* ---- the release register ----
   * MONOTONE, like the health register beside it, and written by the OWNER at the one place it drains
   * this unit's release/burst queues (the growth phase) — never by the module, which is why `no_fire`
   * and `deny release` cannot be talked out of. */
  void NoteWeaponRelease() { ReleasedWeapon_ = true; }
  bool ReleasedWeapon() const { return ReleasedWeapon_; }

  /* ---- the two incorruptible judges (fed here, never handed to the module) ---- */
  void SetMissionMonitor(std::unique_ptr<FBMissionMonitor> monitor) { Mission_ = std::move(monitor); }
  const FBFlightMonitor &FlightMonitor() const { return Flight_; }
  const FBMissionMonitor *MissionMonitor() const { return Mission_.get(); }   /* null: no objectives */

  /* ---- telemetry ----
   * Registers this unit's sources in fixed column order; a null sink leaves the bus a cheap no-op. */
  void StartTelemetry(FBTelemetrySink *sink);

private:
  /* ---- THE TICK, AND IT IS NOT PUBLIC ----------------------------------------------------------
   * Everything a per-frame step consists of — ground truth, the airframe, the barrier, the judges,
   * the telemetry sample — is reachable from ONE friend, missions/FBMissionSim, which is the ONE
   * object that owns a simulation loop. A client that could call these could write a second loop and
   * forget the end rule in it, which is precisely what happened in the browser: it flew a CFIT'd F-16
   * on for as long as the tab was open. THE WRITE GATE IS THE TYPE, the same construction
   * core/FBSystemHealth uses against self-repair. doc/units-and-missions.md §7. */
  friend class Missions::FBMissionSim;

  /* The module cycles its own FDM + systems; `units` reaches only its sensor slots. */
  void Run(double dt, const FBUnitRegistry *units, const World::FBWorld *world) override;

  /* The tick barrier. Called for EVERY unit only once every unit has run — that ORDERING, not this
   * function, is what makes a multi-unit tick order-independent. */
  void PublishPose();

  /* One FDM step so a client's first frame reads a filled state (boot only). Nothing to step without
   * an airframe — the spawn pose is already the whole truth about such a unit. */
  void PrimeState() { if (Fdm_) Fdm_->Step(St_); PublishPose(); }

  /* An unresolved sample keeps the last good value: ONE number reaches both JSBSim's contact floor and
   * the module's HUD/radar-alt path, so the two can never disagree about where the ground is. */
  void UpdateGroundAsl(double sampleM);

  /* The air mass this unit is flying through, sampled by its OWNER from the weather hook and pushed
   * into JSBSim before the substeps. Nothing above the FDM is told: a pilot experiences wind as drift
   * on his instruments, exactly as he would in the aircraft. */
  void UpdateWind(const FBWindNed &wind);

  /* The cloud decks over this unit's position, sampled by its OWNER from the same weather hook and
   * handed to the MODULE (FBModule::SetCloudSky) rather than to the FDM: wind is physics, cloud is
   * something a SENSOR looks through. Only a module with such a sensor does anything with it. */
  void UpdateSky(const FBCloudSky &sky) { Module_->SetCloudSky(sky); }

  /* The sun and moon over this unit at the mission clock's instant, sampled by its OWNER on the same
   * decision tick as the sky. Only called for a mission that DECLARES a `time`; without one no unit
   * has a clock at all, and FBEnvironmentBlock stays Invalid exactly as it was. */
  void UpdateSolar(const FBSolar &solar) { Module_->SetSolar(solar); }

  /* Feeds both judges this tick's observed truth. A physical K.O. is entered in the DAMAGE REGISTER
   * (the loop asks that, not this unit's aircraft monitor) and takes the one consequence it always had
   * — engine cutoff; JSBSim's ground reactions do the rest, no freeze. `roster` is the observed state
   * of the OTHER units a combat objective is judged against, empty for a mission that declares none. */
  bool RunMonitors(double simT, const FBMissionRoster &roster = FBMissionRoster{});

  /* The run ended without a verdict — ask the mission judge (a `survive` objective can only be answered
   * here). No-op for an already-concluded or absent monitor. */
  bool FinalizeMission(double simT, const FBMissionRoster &roster);

  /* Generic envelope diagnostics (stall/overspeed/sink), latched per unit rather than per run. */
  void CheckEnvelope();

  void SampleTelemetry(double simT) { Bus_.Tick(simT); }

  void ApplyDamageToAirframe();   /* health register -> JSBSim, the only place damage becomes physics */
  FBMissionMonitorSample BuildMissionSample(const FBMissionRoster &roster) const;

  /* DECLARATION ORDER IS LOAD-BEARING: the airframe outlives the module that only borrows it, and each
   * telemetry source is declared below everything it borrows. */
  std::unique_ptr<Fdm::FBFdm> Fdm_;
  std::unique_ptr<Modules::FBModule> Module_;
  Fdm::fb_fdm_state St_;
  FBUnitPose Pose_;                    /* published: the last completed tick */
  FBUnitSignature Sig_;                /* published with it */
  std::string LogLabel_;
  double GroundAslM_;
  /* Last vector handed to the FDM, re-set only when it changes — and STILL AIR is the state JSBSim
   * boots in, so a calm run never writes this channel at all. */
  FBWindNed Wind_;
  FBSystemHealth Health_;              /* written only by core/FBDamageModel */
  Fdm::FBFdmTelemetrySource FdmSrc_;
  FBStateBusTelemetry BusSrc_;
  FBSystemHealthTelemetry HealthSrc_;
  FBTelemetryBus Bus_;
  FBFlightMonitor Flight_;
  std::unique_ptr<FBMissionMonitor> Mission_;   /* absent for a unit with no mission to judge */
  bool WarnedStall_ = false, WarnedOverspeed_ = false, WarnedSink_ = false;
  bool Active_ = true;
  bool ReleasedWeapon_ = false;
};

/* A mission's cast in declaration order; index 0 is the PRIMARY actor (canonical telemetry name, the
 * camera's eye). Every client that runs a sim loop owns one. */
using FBActorList = std::vector<std::unique_ptr<FBSimUnit>>;

} // namespace FlightBox::Units
#endif
