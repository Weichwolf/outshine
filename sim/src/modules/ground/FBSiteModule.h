/* FlightBox — FBSiteModule: an AIR-DEFENCE POSITION. ONE CLASS, N CATALOGUE ROWS (core/FBSite.h) — the
 * same value-type-over-class decision modules/stores and modules/missile make, and the reason nine
 * systems are nine rows rather than nine classes.
 *
 * IT IS AN FBModule AND IT IS NOT A MODULE IN THE DOCUMENTATION SENSE: no JSBSim deck, no avionics bus
 * worth the name, no pilot phase machine, no reference base of its own. What it shares with a flown
 * airframe is the whole of units/FBSimUnit — identity, team, published pose and signature, the health
 * register, the damage model, the telemetry file, the mission judge and the `.fbm` `unit` block —
 * because that machinery is about being IN THE WORLD, not about flying.
 *
 * FOUR DETECTORS, ALL OF THEM EXISTING CLASSES, and that is the anti-cheat argument in one line: the
 * two radars are configured instances of modules/ground/FBSiteRadar (itself sensors/FBRadarSystem), the
 * eye is sensors/FBVisualSystem verbatim and the passive receiver sensors/FBRwrSystem verbatim. Not one
 * of them adds an `#include "FBUnitRegistry.h"`, so the RESTRICTED list does not grow.
 *
 * TWO BUSES, and it is not an accident: FBState carries ONE radar block and a block has ONE writer, so
 * a position with two antennas radiating at once needs a second one. The acquisition set writes
 * State_, the fire control writes TrackBus_, and the engagement machine reads both.
 * doc/modules/ground/module.md */
#ifndef FBSITEMODULE_H
#define FBSITEMODULE_H

#include "FBModule.h"
#include "FBSite.h"
#include "FBSiteFireControl.h"
#include "FBSiteRadar.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBSiteModule : public FBModule {
public:
  explicit FBSiteModule(const FBSiteSpec &spec);

  const FBSiteSpec &Spec() const { return Spec_; }

  /* The EMPTY model name is the signal to the spawn path that there is nothing to load, so AttachFdm is
   * never called — which is also what makes the ground-launcher exemption unreachable for an airframe:
   * the two states are mutually exclusive by construction (weapons/FBStoresSystem). */
  const char *FdmModelName() const override { return ""; }
  void AttachFdm(Fdm::FBFdm &fdm) override { (void)fdm; }
  Units::FBUnitKind UnitKind() const override { return Units::FBUnitKind::Ground; }

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  const FBDamageLayout &DamageLayout() const override { return Spec_.Layout; }

  /* THE SECOND ANTENNA, the one core change this class asks for. It is derived from the fire-control
   * SET's own pattern, exactly as beam 0 is from the acquisition set's, so what the position radiates
   * and what its antennas are doing cannot diverge. */
  FBEmitterSignature TrackBeamEmission() override {
    return Fc_.TrackRadiating() ? Track_.Emission() : FBEmitterSignature{};
  }

  /* WHAT THIS POSITION PUTS ON THE NET, and the terminal is the ONE gate: a position with no net has no
   * terminal, a position whose terminal was shot away transmits nothing. That is FBSystemId::Datalink
   * doing its existing job — no new type, no new friend, no new id. */
  FBNetReport NetReport() override {
    return Datalink_.Transmitting() ? Fc_.NetReport() : FBNetReport{};
  }
  FBTelemetrySource *NetTelemetry() override { return NetTerminal_ ? &Fc_.NetTelemetrySource() : nullptr; }

  Systems::FBAutopilot &Autopilot() override { return AP_; }
  Systems::FBFlightControl &FlightControl() override { return FC_; }
  FBSiteFireControl &PilotSystem() override { return Fc_; }   /* covariant: FBPilot& on the base */
  Systems::FBAirframeControls &Controls() override { return Ctrl_; }
  Systems::FBDisplaySystem &Displays() override { return Disp_; }
  Systems::FBAirDataSystem &AirDataSystem() override { return AirData_; }
  Systems::FBNavSystem &NavSystem() override { return Nav_; }
  Systems::FBWarningSystem &WarningSystem() override { return Warn_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  Sensors::FBDatalinkSystem &Datalink() override { return Datalink_; }
  FBSiteRadar &Radar() override { return Search_; }           /* covariant: the ACQUISITION set */
  Sensors::FBRwrSystem &Rwr() override { return Esm_; }        /* the passive receiver, the EMCON cue */
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  Sensors::FBVisualSystem &Visual() override { return Optics_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Rails_; }
  /* The MAGAZINE, not the rail: this position reloads. */
  int MaxReleases() override { return Fc_.RoundsLeft(); }
  Weapons::FBGunSystem &Guns() override { return Gun_; }
  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return 0; }

  void SetUnitIdentity(int unitId, FBUnitTeam team) override {
    Search_.SetIdentity(unitId, team);
    Track_.SetIdentity(unitId, team);
    Esm_.SetIdentity(unitId, team);
    Datalink_.SetIdentity(unitId, team);   /* whose net this position is on — a net is one side's */
    Optics_.SetIdentity(unitId);   /* the id only — an eye is given no team to read */
    Rails_.SetUnitId(unitId);
    Gun_.SetUnitId(unitId);
  }

  /* The eye's own weather and the clock that decides whether it works at all. Without a mission `time`
   * the environment block stays Invalid and the channel runs as a clear day, exactly as it does for
   * every aircraft written before the clock existed. */
  void SetCloudSky(const FBCloudSky &sky) override { Optics_.SetSky(sky); }
  void SetSolar(const FBSolar &solar) override { FBSolarToEnv(solar, State_); }
  void SetGroundAsl(float m) override { (void)m; }
  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }

  /* The SIX mission keys of doc/modules/ground/module.md §Spec 9 and nothing else. What a position IS,
   * its module name says; what its COMMANDER decided is these. */
  bool ApplySetup(const std::string &key, const std::string &value) override;

private:
  void ServiceCommands(FBCommandGroup group);
  void ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome, FBCommandReason &reason);

  const FBSiteSpec &Spec_;
  double SimTimeS_ = 0.0;
  /* Set by the runner-generated `net_link` key and by nothing else: this position is on a net. Without
   * it the terminal stays unpowered, the slot is never cycled and the position is byte-identical to one
   * from before nets existed. */
  bool NetTerminal_ = false;

  Systems::FBAutopilot AP_;
  Systems::FBFlightControl FC_;
  FBSiteRadar Search_;   /* the acquisition set: writes State_.Radar */
  FBSiteRadar Track_;    /* the fire-control set: writes TrackBus_.Radar */
  Sensors::FBVisualSystem Optics_;
  Sensors::FBRwrSystem Esm_;
  Sensors::FBIrstSystem Irst_;
  Sensors::FBDatalinkSystem Datalink_;
  Sensors::FBCountermeasureSystem Cm_;
  Weapons::FBStoresSystem Rails_;
  Weapons::FBGunSystem Gun_;
  FBSiteFireControl Fc_;
  Systems::FBAirframeControls Ctrl_;
  Systems::FBDisplaySystem Disp_;
  Systems::FBAirDataSystem AirData_;
  Systems::FBNavSystem Nav_;
  Systems::FBWarningSystem Warn_;
  Systems::FBRadarAltimeter RadarAlt_;
  FBCommandBus Cmds_;
  FBFlightPlan Plan_;
  FBState State_{};      /* the position's bus: acquisition, eye, receiver, rails */
  FBState TrackBus_{};   /* the fire-control set's own, for the reason in the class banner */
  Systems::FBGuidance LastG_{};
};

} // namespace FlightBox::Modules
#endif
