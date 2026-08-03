/* FlightBox — FBModule: one controllable module's per-frame update, held POLYMORPHICALLY by the client.
 * A module owns its systems and cycles them internally, each at its own rate; it never calls a peer's
 * Run(). Every accessor below is on the BASE so a caller that only knows "some FBModule" can wire,
 * command and observe it without naming a concrete type. doc/units-and-missions.md §5. */
#ifndef FBMODULE_H
#define FBMODULE_H

#include <cstdlib>
#include <string>
#include "FBAirDataSystem.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBCommandBus.h"
#include "FBDamageModel.h"
#include "FBCountermeasureSystem.h"
#include "FBDatalinkSystem.h"
#include "FBDisplaySystem.h"
#include "FBEphemeris.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBGunSystem.h"
#include "FBInputSystem.h"
#include "FBIrstSystem.h"
#include "FBNavSystem.h"
#include "FBPilot.h"
#include "FBRadarAltimeter.h"
#include "FBRadarSystem.h"
#include "FBRwrSystem.h"
#include "FBStoresSystem.h"
#include "FBVisualSystem.h"
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

  /* THE REGISTRY KEY this module was created under ("f16", "mig29", "aim120"), stamped once by
   * FBModuleRegistry::Create and by nothing else — so the key a mission spells and the key a module
   * answers with cannot be two different strings. Empty for a module built directly (the test
   * harnesses), and an eye then never names its type: the registry is the only source. */
  const std::string &TypeName() const { return TypeName_; }

  /* Ground also declares that there is NO airframe, which the empty FdmModelName() then says where the
   * spawn path reads it. Weapon is NOT set here: a store is a kind by having been RELEASED, which is
   * the releasing path's statement. */
  virtual Units::FBUnitKind UnitKind() const { return Units::FBUnitKind::Aircraft; }

  /* IS THIS UNIT IN THE AIR, asked of a module that has no airframe to ask. A unit without an FBFdm
   * reports weight on wheels BY CONSTRUCTION (units/FBSimUnit::BuildMissionSample), which is right for
   * a ground position — a launcher really is on the ground — and wrong for a kinematically integrated
   * tanker at 25 000 ft. Default false, so every module written before this existed is unchanged.
   * doc/modules/air/module.md §Gaps collision 1. */
  virtual bool Airborne() const { return false; }

  /* WHO this module is flying — wiring, once, like AttachFdm; needed by any slot that observes OTHER
   * units (the datalink must recognise its own PPLI and know whose net it is on). */
  virtual void SetUnitIdentity(int unitId, FBUnitTeam team) { (void)unitId; (void)team; }

  /* WHICH FLIGHT it flies in, same wiring step — and deliberately NOT virtual: a flight is something
   * the PILOT holds (it decides where to sit and whom to shoot), so every module that has one gets
   * this for free and a module whose pilot slot is the NoOp default is untouched by it. */
  void SetFlight(const FBFlightId &flight) { PilotSystem().SetFlight(flight); }
  FBFlightReport FlightReport() { return PilotSystem().FlightReport(); }

  /* ---- Battle damage: the module READS its own health and never writes it — every mutator on
   * FBSystemHealth is private to core/FBDamageModel. Unattached reads as fully intact. ---- */
  void AttachHealth(const FBSystemHealth &health) { Health_ = &health; }
  FBHealthState HealthOf(FBSystemId id) const {
    return Health_ ? Health_->State(id) : FBHealthState::Intact;
  }
  bool SystemWorking(FBSystemId id) const { return HealthOf(id) != FBHealthState::Failed; }
  bool SystemDegraded(FBSystemId id) const { return HealthOf(id) == FBHealthState::Degraded; }
  /* HOW OFTEN THIS UNIT HAS BEEN HIT — monotone, and the only thing a module can honestly call "we are
   * under attack" without reading the world. doc/air-to-ground.md §5.1. */
  int OwnHits() const { return Health_ ? Health_->Hits() : 0; }

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
  /* The pilot's own EYES — a sensor slot and not a pilot feature, because a "visual" derived outside
   * the perception boundary is the cheat the whole architecture exists to prevent (doc/sensors.md §9.1). */
  virtual Sensors::FBVisualSystem &Visual() = 0;
  virtual Sensors::FBCountermeasureSystem &Countermeasures() = 0;
  /* THE SEAT A HUMAN CAN TAKE, and null is the honest default: a missile, a store and a ground battery
   * have no cockpit, so there is nothing for a pair of hands to hold. A module that HAS one hands out
   * the slot it already cycles — the human then works the identical bus, latency and rejection
   * catalogue its own pilot works, because it is the same hardware. doc/player-layer.md §10.2. */
  virtual Systems::FBInputSystem *HumanInput() { return nullptr; }

  /* The client drains these two queues: a released store and a fired burst become part of the world,
   * and only the client may create units (fdm/FBFdmBoot.h) or decide what a round hits. */
  virtual Weapons::FBStoresSystem &Stores() = 0;
  virtual Weapons::FBGunSystem &Guns() = 0;
  virtual const FBState &Telemetry() const = 0;

  /* THE SECOND ANTENNA, published beside Radar()'s into FBUnitSignature::Radar[1]. Silent by default,
   * which is every airframe: a jet has one set, and its signature is the scalar it always was. A ground
   * battery fills it, because its acquisition radar keeps sweeping while its fire control stares —
   * doc/modules/ground/module.md §Spec 6. Non-const like the slot accessors: it is derived from what
   * the set is doing, not stored. */
  virtual FBEmitterSignature TrackBeamEmission() { return {}; }

  /* WHAT THIS UNIT PUTS ON AN AIR-DEFENCE NET, published beside the two beams into
   * FBUnitSignature::Net. Silent by default, which is every airframe: a jet is on no ground net, and
   * the field a receiver reads then says `Reporting == false` exactly as it did before it existed. A
   * position that IS on one fills it from its own acquisition set — a POINT, never a track.
   * doc/air-defence-network.md §3. */
  virtual FBNetReport NetReport() { return {}; }

  /* WHOSE PICTURE THE TACTICAL MAP IS, said by the unit itself: the callsign of the control node this
   * unit's mission put it under. Empty = on no net, or the node itself — and a client that finds no
   * node draws the unit's own picture, which is the smaller claim of the two. It is mission data read
   * back, not a sensor: the callsign was written in the `net` block. doc/player-layer.md §9.1. */
  virtual const char *NetControlNode() const { return ""; }

  /* THE JAMMER, as a property of a unit rather than a unit type — one radius, published like the radar
   * cross-section beside it and for the same reason. 0 = not a jammer, which is every module written
   * before this existed. doc/air-defence-network.md §6. */
  double CommJamM() const { return CommJamM_; }

  /* ONE MORE TELEMETRY SOURCE a module may own, appended after everything FBSimUnit registers by name.
   * Null is the default and is every airframe; a netted position returns its `net` source. Null-return
   * keeps every existing telemetry.csv identical column for column. */
  virtual FBTelemetrySource *NetTelemetry() { return nullptr; }

  /* HOW MANY UNITS THIS MODULE CAN STILL PUT INTO THE WORLD — the client sizes its per-actor buffers
   * from it before the first tick, and a store that found no room would be a store that vanished.
   * Default: what is on the rails, which for an aircraft is the whole answer. A ground position
   * reloads ONE rail out of a magazine, so its answer is the magazine and not the rail. */
  virtual int MaxReleases() { return Stores().LoadedCount(); }

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

  /* Where the sun and the moon stand over this unit, from the mission CLOCK its owner samples — the
   * sibling of SetCloudSky, same cadence, same reason. A mission that declares no `time` never calls
   * it, so nothing is published and nothing changes for every mission written before the clock. */
  virtual void SetSolar(const FBSolar &solar) { (void)solar; }

  /* The launch programming a released store is handed at separation, once, before its first tick.
   * Default: an unguided store has nothing to program. */
  virtual void ProgramRelease(const FBStoreRelease &rel) { (void)rel; }

  /* One `set <key> <value>` mission line, applied in the spawn IC window — the MODULE interprets its
   * own keys. False iff unrecognized, which the caller turns into a mission FAIL, never a silent no-op. */
  virtual bool ApplySetup(const std::string &key, const std::string &value) = 0;

protected:
  /* The ONE mission key that is a fact about a UNIT and not about an airframe, so it is answered here
   * once instead of by every module that could carry a jamming pod. A module opts in by calling it from
   * its own ApplySetup; one that does not is simply not a jammer. Returns false for any other key. */
  bool ApplyJammerSetup(const std::string &key, const std::string &value) {
    if (key != "jam_comm_m") return false;
    char *end = nullptr;
    double m = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !(m >= 0.0)) return false;
    CommJamM_ = m;
    return true;
  }

private:
  friend class FBModuleRegistry;   /* the ONE writer of TypeName_, at creation */
  void SetTypeName(std::string name) { TypeName_ = std::move(name); }

  const FBSystemHealth *Health_ = nullptr;   /* borrowed, read-only — see AttachHealth */
  std::string TypeName_;
  double CommJamM_ = 0.0;
};

} // namespace FlightBox::Modules
#endif
