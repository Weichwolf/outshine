/* FlightBox — FBModule: one controllable module's per-frame update, held POLYMORPHICALLY by the client.
 * A module owns its systems and cycles them internally, each at its own rate; it never calls a peer's
 * Run(). Every accessor below is on the BASE so a caller that only knows "some FBModule" can wire,
 * command and observe it without naming a concrete type. doc/units-and-missions.md §5. */
#ifndef FBMODULE_H
#define FBMODULE_H

#include <string>
#include "FBAirDataSystem.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBCommandBus.h"
#include "FBDamageModel.h"
#include "FBCountermeasureSystem.h"
#include "FBDatalinkSystem.h"
#include "FBDisplaySystem.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBGunSystem.h"
#include "FBIrstSystem.h"
#include "FBNavSystem.h"
#include "FBPilot.h"
#include "FBRadarAltimeter.h"
#include "FBRadarSystem.h"
#include "FBRwrSystem.h"
#include "FBStoresSystem.h"
#include "FBWarningSystem.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBUnit.h"
#include "FBFdm.h"

namespace FlightBox::World { class FBWorld; }          /* borrowed only — the module never owns it */
namespace FlightBox::Units { class FBUnitRegistry; }   /* likewise: the cast of the world, for the module's SENSORS only */

namespace FlightBox::Modules {

class FBModule {
public:
  virtual ~FBModule() = default;

  /* Binds the module to the airframe it flies, ONCE, before the first Run(). Registry factories take no
   * arguments, so this is the module's constructor injection — and the point at which it can hand a
   * real `FBFdm&` to the systems whose association to an airframe IS fixed. The FDM is BORROWED. */
  virtual void AttachFdm(Fdm::FBFdm &fdm) = 0;

  /* The JSBSim model this module flies — module-owned, because only the module knows which aircraft.xml
   * its systems/gains were written against; the registry name stays a pure key. */
  virtual const char *FdmModelName() const = 0;

  /* Ground also declares that there is NO airframe, which the empty FdmModelName() then says where the
   * spawn path reads it. Weapon is NOT set here: a store is a kind by having been RELEASED, which is
   * the releasing path's statement. */
  virtual Units::FBUnitKind UnitKind() const { return Units::FBUnitKind::Aircraft; }

  /* WHO this module is flying — wiring, once, like AttachFdm; needed by any slot that observes OTHER
   * units (the datalink must recognise its own PPLI and know whose net it is on). */
  virtual void SetUnitIdentity(int unitId, FBUnitTeam team) { (void)unitId; (void)team; }

  /* ---- Battle damage: the module READS its own health and never writes it — every mutator on
   * FBSystemHealth is private to core/FBDamageModel. Unattached reads as fully intact. ---- */
  void AttachHealth(const FBSystemHealth &health) { Health_ = &health; }
  FBHealthState HealthOf(FBSystemId id) const {
    return Health_ ? Health_->State(id) : FBHealthState::Intact;
  }
  bool SystemWorking(FBSystemId id) const { return HealthOf(id) != FBHealthState::Failed; }
  bool SystemDegraded(FBSystemId id) const { return HealthOf(id) == FBHealthState::Degraded; }

  /* WHAT A RADAR GETS BACK from this airframe, m^2. Module data for the same reason the damage layout
   * is: how big an echo an aircraft makes is a property of the aircraft, and a radar that carried a
   * table of it would be reading identity out of the registry. 0 = not declared (stores, ground
   * targets, and any module written before cross-sections existed), which the radar reads as "no
   * scaling" — exactly the behaviour it had before. doc/sensors.md, Spec. */
  virtual double RadarCrossSectionM2() const { return 0.0; }

  /* WHERE this airframe's systems sit, for core/FBDamageModel — the module supplies the table, the core
   * applies it. Empty default: a released store has nothing to lose piecewise. */
  virtual const FBDamageLayout &DamageLayout() const {
    static const FBDamageLayout kNone{};
    return kNone;
  }

  /* Advances the module's FDM at its own substep rate for `dt` wall-seconds; `st` is the shared live
   * state the client reads back. `units`/`world` are borrowed, may be null, and the module hands each
   * one only to the slots entitled to it. */
  virtual void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
                   const World::FBWorld *world = nullptr) = 0;

  /* The three REAL system slots; a concrete module may covariantly return a more-derived pilot type. */
  virtual Systems::FBAutopilot &Autopilot() = 0;
  virtual Systems::FBFlightControl &FlightControl() = 0;
  virtual Pilot::FBPilot &PilotSystem() = 0;

  virtual Systems::FBAirframeControls &Controls() = 0;
  virtual Systems::FBDisplaySystem &Displays() = 0;
  virtual Systems::FBAirDataSystem &AirDataSystem() = 0;
  virtual Systems::FBNavSystem &NavSystem() = 0;
  virtual Systems::FBWarningSystem &WarningSystem() = 0;
  virtual Systems::FBRadarAltimeter &RadarAltimeter() = 0;
  virtual FBCommandBus &Commands() = 0;
  virtual Sensors::FBDatalinkSystem &Datalink() = 0;
  virtual Sensors::FBRadarSystem &Radar() = 0;
  virtual Sensors::FBRwrSystem &Rwr() = 0;
  virtual Sensors::FBIrstSystem &Irst() = 0;
  virtual Sensors::FBCountermeasureSystem &Countermeasures() = 0;
  /* The client drains these two queues: a released store and a fired burst become part of the world,
   * and only the client may create units (fdm/FBFdmBoot.h) or decide what a round hits. */
  virtual Weapons::FBStoresSystem &Stores() = 0;
  virtual Weapons::FBGunSystem &Guns() = 0;
  virtual const FBState &Telemetry() const = 0;

  /* Diagnostics of the INTERFACE, not of one airframe: every module issues guidance and steps an FDM. */
  virtual const Systems::FBGuidance &LastGuidance() const = 0;
  virtual int LastSubsteps() const = 0;

  virtual FBFlightPlan &FlightPlan() = 0;
  virtual void SetRunway(const FBRunway &rwy) = 0;
  /* The client's elevation-hook sample, forwarded so e.g. FBRadarAltimeter never re-queries terrain. */
  virtual void SetGroundAsl(float m) = 0;

  /* The weather the OWNER already resolved for this unit, in the same role as SetGroundAsl: one
   * sample per decision tick, handed down instead of queried a second time. Only a module with a
   * sensor that cares about it does anything with it — the default is deliberately a no-op, so a
   * module without one is unchanged by the existence of weather. */
  virtual void SetCloudSky(const FBCloudSky &sky) { (void)sky; }

  /* The launch programming a released store is handed at separation, once, before its first tick.
   * Default: an unguided store has nothing to program. */
  virtual void ProgramRelease(const FBStoreRelease &rel) { (void)rel; }

  /* One `set <key> <value>` mission line, applied in the spawn IC window — the MODULE interprets its
   * own keys. False iff unrecognized, which the caller turns into a mission FAIL, never a silent no-op. */
  virtual bool ApplySetup(const std::string &key, const std::string &value) = 0;

private:
  const FBSystemHealth *Health_ = nullptr;   /* borrowed, read-only — see AttachHealth */
};

} // namespace FlightBox::Modules
#endif
