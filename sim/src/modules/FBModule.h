/* FlightBox — FBModule: one controllable module's per-frame update. The App holds each module
 * POLYMORPHICALLY through this interface (selection is a runtime concern — today the F-16 is the one
 * registered module, but the dispatch is real, not a shortcut to it) and cycles Run() in a fixed
 * order; a module never calls a peer's Run(). A module owns its own systems (guidance/FCS/planner/
 * input/propulsion/displays/sensors/weapons/defensive/comms) and cycles them internally, each at its
 * own rate — the heterogeneous-rate scheduling is module-internal, not part of this interface.
 *
 * Every module composes the SAME system categories (doc/f16/ inventory) — CLAUDE.md's "FBCore ->
 * Interface -> Default-Implementation -> modul-spezifischer Override". The generic accessors below are
 * that seam surfaced on the BASE: a caller that only knows "some FBModule" (FBMissionRunner.h's
 * headless mission loop, resolved via FBModuleRegistry by name, never a concrete module type) still
 * gets ground-spawn, telemetry-bus registration, and HUD wiring through them, with zero knowledge of
 * which concrete module it is driving. */
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

namespace FlightBox {

class FBWorld;          /* borrowed only — sensors/weapons/defensive query it, the module never owns it */
class FBUnitRegistry;   /* likewise: the cast of the world, for the module's SENSORS only */

class FBModule {
public:
  virtual ~FBModule() = default;

  /* Binds the module to the airframe it flies — called ONCE by the owner (App/mission runner today,
   * units/FBUnit later) right after the FDM is spawned and before the first Run(). The FDM is BORROWED
   * and outlives the module; the module never owns or spawns one (it cannot: the IC lives behind
   * fdm/FBFdmBoot.h, which no module includes). Modules are produced by FBModuleRegistry's name->factory
   * with no arguments, so this is the module's equivalent of constructor injection: one wiring call,
   * permanent for the module's life, and the point at which the module can hand a real `FBFdm&` to the
   * systems whose association to an airframe IS fixed (FBJsbsimAirframeControls). */
  virtual void AttachFdm(FBFdm &fdm) = 0;

  /* The JSBSim model directory/XML this module flies ("Flugzeug = Modul + JSBSim-Modell", CLAUDE.md —
   * two parts, so the mission's `module <name>` line stays a pure REGISTRY key). Owned by the module
   * because only the module knows which aircraft.xml its systems/gains were written against; the boot
   * path (FBMissionBoot.h) asks for it instead of reusing the registry name as a directory name. */
  virtual const char *FdmModelName() const = 0;

  /* WHAT KIND OF WORLD ENTITY this module is (units/FBUnit.h). Aircraft by default, because everything
   * that flies itself is one; a module returning FBUnitKind::Ground declares that it is a static object
   * and — the part that matters structurally — that it has no airframe at all, which its empty
   * FdmModelName() then says in the one place the spawn path reads. Weapon is NOT set here: a store is
   * a kind by virtue of having been RELEASED, which is the releasing path's statement and not the
   * module's (app/FBMissionBoot.h). */
  virtual FBUnitKind UnitKind() const { return FBUnitKind::Aircraft; }

  /* WHICH model root FdmModelName lives under. true (the default, and every aircraft in this tree) =
   * the pinned, read-only JSBSim submodule; false = FlightBox's own model assets, which is where a model
   * has to live when the submodule carries none and may not be added to (CLAUDE.md Prinzip 1 — the
   * AIM-120 is the first). The MODULE states it for the same reason it states the model name: only the
   * module knows which model its systems were written against, and the boot path (app/FBModelRoots.h)
   * turns the answer into a path per link target. */
  virtual bool FdmModelVendored() const { return true; }

  /* WHO this module is flying, from the unit that owns it (units/FBSimUnit's constructor) — wiring,
   * once, like AttachFdm. A module needs its own identity the moment it carries a system that observes
   * OTHER units: the datalink has to recognise its own PPLI in the registry and know whose net it is
   * on. Default: nothing to identify. */
  virtual void SetUnitIdentity(int unitId, FBUnitTeam team) { (void)unitId; (void)team; }

  /* ---- Battle damage: the module READS its own health and never writes it ----
   * The owning unit hands over a CONST reference to its health register once, like AttachFdm
   * (units/FBSimUnit's constructor). A module uses it to decide whether to cycle a slot at all and what
   * a degraded system may still do; it cannot change a single state, because every mutator on
   * FBSystemHealth is private to core/FBDamageModel (core/FBSystemHealth.h's banner). Until a unit
   * attaches one — a test harness, the browser's pre-mission boot — Health() reports everything intact,
   * which is what an unattached module has always effectively been. */
  void AttachHealth(const FBSystemHealth &health) { Health_ = &health; }
  FBHealthState HealthOf(FBSystemId id) const {
    return Health_ ? Health_->State(id) : FBHealthState::Intact;
  }
  bool SystemWorking(FBSystemId id) const { return HealthOf(id) != FBHealthState::Failed; }
  bool SystemDegraded(FBSystemId id) const { return HealthOf(id) == FBHealthState::Degraded; }

  /* WHERE this airframe's systems sit, for the core's damage resolution (core/FBDamageModel — the
   * module supplies the table, the core applies it and is the only thing that may write a verdict from
   * it). The default is an empty layout: a module that has declared no zones — a released store, whose
   * own end is its detonation — has nothing to lose piecewise. */
  virtual const FBDamageLayout &DamageLayout() const {
    static const FBDamageLayout kNone{};
    return kNone;
  }

  /* Advances the module's FDM at its own fixed substep rate for `dt` wall-seconds. `st` is the
   * shared live FDM state the caller (App) reads back for camera/HUD/telemetry. `units` is the borrowed
   * unit registry (units/FBUnitRegistry — the last completed tick's snapshots, for SENSORS only),
   * `world` the borrowed terrain side; both may be null in a client that has none — never global
   * access, and the module hands each one only to the slots entitled to it. */
  virtual void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
                   const FBWorld *world = nullptr) = 0;

  /* Guidance / FCS / the mission-level pilot brain — the three REAL system slots (CLAUDE.md), each
   * with its one module-overridable point; PilotSystem() returns the base FBPilot& (a concrete module
   * may covariantly return a more-derived type, e.g. FBF16Pilot&). */
  virtual FBAutopilot &Autopilot() = 0;
  virtual FBFlightControl &FlightControl() = 0;
  virtual FBPilot &PilotSystem() = 0;

  /* Airframe-agnostic slots every module fills the same way: gear/brakes/engine-cutoff, the HUD
   * (FBRenderer::SetHudDisplay), the ADC telemetry source the mission Bus registers, and the module's
   * own HUD-ready FBState snapshot. */
  virtual FBAirframeControls &Controls() = 0;
  virtual FBDisplaySystem &Displays() = 0;
  virtual FBAirDataSystem &AirDataSystem() = 0;
  virtual FBNavSystem &NavSystem() = 0;
  /* The warning set (systems/FBWarningSystem) and the radar altimeter: surfaced on the base for the
   * same generic reason the terminal and the radar are — the client registers the one's telemetry and
   * a mission's setup line switches the other's power, and neither needs to know the concrete module. */
  virtual FBWarningSystem &WarningSystem() = 0;
  virtual FBRadarAltimeter &RadarAltimeter() = 0;

  /* The module's avionics COMMAND bus (core/FBCommandBus.h) — the one path from a pilot's intent to a
   * box in this jet, surfaced on the base because the client registers its telemetry (the command
   * stream) exactly as it registers the state-bus sources, knowing nothing about the module. */
  virtual FBCommandBus &Commands() = 0;
  /* The Comms/Datalink slot: the client registers its telemetry and publishes its transmit state into
   * the unit's snapshot (units/FBSimUnit) — both generic, both true of any module with a terminal. */
  virtual FBDatalinkSystem &Datalink() = 0;
  /* The Sensors slot: the active radar. Surfaced on the base for exactly the same two generic reasons
   * the terminal is — the client registers its telemetry and publishes its IFF transponder state into
   * the unit's emission snapshot (units/FBSimUnit) — and for neither of them does the client need to
   * know which set it is. */
  virtual FBRadarSystem &Radar() = 0;
  /* The Defensive slot, which is TWO systems: the warning receiver and the dispenser that answers it.
   * Both are surfaced on the base for the same generic reasons the radar and the terminal are — the
   * client registers their telemetry, and the dispenser's chaff clouds go into the unit's published
   * emission snapshot (units/FBSimUnit) exactly as the radar's beam does. */
  virtual FBRwrSystem &Rwr() = 0;
  virtual FBCountermeasureSystem &Countermeasures() = 0;
  /* The Stores slot: what this module carries and the queue of what it has let go of. Surfaced on the
   * base because the client both registers its telemetry AND drains its releases — the released store
   * becomes a unit of the world, which only the client can create (fdm/FBFdmBoot.h), and it must be
   * able to do that without knowing which module dropped it. */
  virtual FBStoresSystem &Stores() = 0;
  /* The gun slot: this aircraft's internal gun and the queue of bursts it has fired. Surfaced on the
   * base for exactly the reason Stores() is — the client registers its telemetry AND drains its bursts,
   * because the rounds become part of the world (core/FBGunProjectiles) and only the client may fly them
   * and decide what they hit. A module with no gun fitted still has the slot; it simply never produces
   * a burst. */
  virtual FBGunSystem &Guns() = 0;
  virtual const FBState &Telemetry() const = 0;

  /* The two OUTPUTS of the Run() contract above, surfaced for the client's flight/cpuprof telemetry:
   * the guidance command this module last issued, and how many fixed FDM substeps its last Run()
   * actually executed. Every module produces both by construction (it cycles guidance and steps an
   * FDM) — they are diagnostics of the interface, not of one airframe, which is why the browser client
   * can log them without ever naming a concrete module. */
  virtual const FBGuidance &LastGuidance() const = 0;
  virtual int LastSubsteps() const = 0;

  /* Mission boot: the flight plan a ground-spawn assigns and the runway it takes off from/lands on. */
  virtual FBFlightPlan &FlightPlan() = 0;
  virtual void SetRunway(const FBRunway &rwy) = 0;

  /* Ground ASL (m) under the aircraft — the App's elevation-hook sample (FBElevationProvider),
   * forwarded so e.g. FBRadarAltimeter doesn't re-query terrain itself. */
  virtual void SetGroundAsl(float m) = 0;

  /* THE LAUNCH PROGRAMMING a released store is handed at separation (core/FBStore.h's FBStoreRelease):
   * which unit let it go, at what mission time, and — for a guided round — what that unit's fire control
   * had made of the target. Called once by the spawn path (app/FBMissionBoot.h) before the first tick,
   * generically, so the runner never names a weapon type. Default: an unguided store has nothing to
   * program, and the record is simply the reason it exists. */
  virtual void ProgramRelease(const FBStoreRelease &rel) { (void)rel; }

  /* Applies one `set <key> <value>` mission line (doc/mission-format.md) during the spawn IC window,
   * after the FDM IC is established and before the pilot's first tick — the MODULE interprets its own
   * keys (the Runner/Boot only parses the flat KV list, FBMissionBoot.h). Returns false iff `key` is
   * unrecognized by this module, which the caller turns into a mission FAIL (exit 1), never a silent
   * no-op. */
  virtual bool ApplySetup(const std::string &key, const std::string &value) = 0;

private:
  const FBSystemHealth *Health_ = nullptr;   /* borrowed, read-only — see AttachHealth */
};

} // namespace FlightBox
#endif
