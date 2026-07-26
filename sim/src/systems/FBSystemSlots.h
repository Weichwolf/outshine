/* FlightBox — the module system slots a full F-16-class airframe carries beyond guidance/FCS/planning
 * (doc/f16/ INDEX.md inventory): pilot input, propulsion, displays, sensors, weapons, defensive aids,
 * comms/datalink. Each is an INTERFACE + trivial DEFAULT (NoOp) in one class, one class per slot (no
 * interface/default file split for a body this small) — a module overrides Run() once it implements
 * the real system; until then FBF16Module composes these DEFAULTS unmodified, same pattern as
 * FBAutopilot/FBFlightControl. Displays is the one slot with a REAL default (the generic
 * HUD symbology) big enough to earn its own file — see FBDisplaySystem.h.
 *
 * Signatures encode the two contract rules from the doc/f16/ distillate: Sensors WRITE the shared
 * FBState, Displays only READ it (no display queries a sensor directly); anything that needs to see
 * other traffic or terrain (Sensors/Weapons/Defensive) takes a borrowed, read-only FBWorld pointer —
 * never global access. A weapon system that spawns munitions needs a mutable world path; that comes
 * with the first real implementation. */
#ifndef FBSYSTEMSLOTS_H
#define FBSYSTEMSLOTS_H

#include "FBDisplaySystem.h"
#include "FBMasterMode.h"
#include "FBState.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBWorld;   /* borrowed only; Sensors/Weapons/Defensive query it, none of them own it */

/* HOTAS (SSC+TQS) + ICP: routes stick/switch events by the active master mode — input routing is
 * module authority, not global. NoOp until a module binds real input. */
class FBInputSystem {
public:
  virtual ~FBInputSystem() = default;
  virtual void Run(FBMasterMode mode, double dt) { (void)mode; (void)dt; }
};

/* Engine-SYSTEM logic above the raw FDM (F110+DEEC state, BINGO/JOKER fuel calls, EPU) — JSBSim's own
 * propulsion model already drives thrust; this is where engine-management logic layers on. NoOp. */
class FBPropulsionSystem {
public:
  virtual ~FBPropulsionSystem() = default;
  virtual void Run(const fb_fdm_state &s, double dt) { (void)s; (void)dt; }
};

/* FCR radar (A-A/A-G), TGP, HMCS, INS (with align/drift), TACAN, ILS: WRITES FBState for displays,
 * reads the world (terrain masking, other units) through a borrowed pointer. NoOp. */
class FBSensorSystem {
public:
  virtual ~FBSensorSystem() = default;
  virtual void Run(FBState &state, const FBWorld *world, double dt) { (void)state; (void)world; (void)dt; }
};

/* SMS/Stores, CCIP/CCRP/DTOS, gun: mode-gated. Takes the same borrowed, READ-ONLY world pointer every
 * slot gets (see FBModule::Run) — spawning a real munition will need a MUTABLE world path once a
 * weapon system is actually implemented; not built speculatively ahead of that. NoOp. */
class FBWeaponSystem {
public:
  virtual ~FBWeaponSystem() = default;
  virtual void Run(FBMasterMode mode, const FBWorld *world, double dt) { (void)mode; (void)world; (void)dt; }
};

/* RWR + CMDS (chaff/flare): reads the world for other emitters through a borrowed pointer. NoOp. */
class FBDefensiveSystem {
public:
  virtual ~FBDefensiveSystem() = default;
  virtual void Run(const FBWorld *world, double dt) { (void)world; (void)dt; }
};

/* Link-16, IFF, COM1/2: no world/state dependency today. NoOp. */
class FBCommsSystem {
public:
  virtual ~FBCommsSystem() = default;
  virtual void Run(double dt) { (void)dt; }
};

} // namespace FlightBox
#endif
