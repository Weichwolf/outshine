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
#include "FBDisplaySystem.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBPilot.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBFdm.h"

namespace FlightBox {

class FBWorld;   /* borrowed only — sensors/weapons/defensive query it, the module never owns it */

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

  /* Advances the module's FDM at its own fixed substep rate for `dt` wall-seconds. `st` is the
   * shared live FDM state the caller (App) reads back for camera/HUD/telemetry. `world` is a borrowed
   * reference (nullptr where a module has no world-facing systems yet) — never global access. */
  virtual void Run(fb_fdm_state &st, double dt, const FBWorld *world = nullptr) = 0;

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
  virtual const FBState &Telemetry() const = 0;

  /* Mission boot: the flight plan a ground-spawn assigns and the runway it takes off from/lands on. */
  virtual FBFlightPlan &FlightPlan() = 0;
  virtual void SetRunway(const FBRunway &rwy) = 0;

  /* Ground ASL (m) under the aircraft — the App's elevation-hook sample (FBElevationProvider),
   * forwarded so e.g. FBRadarAltimeter doesn't re-query terrain itself. */
  virtual void SetGroundAsl(float m) = 0;

  /* Applies one `set <key> <value>` mission line (doc/mission-format.md) during the spawn IC window,
   * after the FDM IC is established and before the pilot's first tick — the MODULE interprets its own
   * keys (the Runner/Boot only parses the flat KV list, FBMissionBoot.h). Returns false iff `key` is
   * unrecognized by this module, which the caller turns into a mission FAIL (exit 1), never a silent
   * no-op. */
  virtual bool ApplySetup(const std::string &key, const std::string &value) = 0;
};

} // namespace FlightBox
#endif
