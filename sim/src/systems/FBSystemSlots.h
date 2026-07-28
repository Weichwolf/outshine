/* FlightBox — die noch NoOp gebliebenen Systemslots eines Moduls (Input/HOTAS, Propulsion, Weapons).
 * Slots, deren Default REAL wurde, sind in eigene Dateien herausgewachsen.
 * doc/flightbox/systems.md, Abschnitt 10. */
#ifndef FBSYSTEMSLOTS_H
#define FBSYSTEMSLOTS_H

#include "FBCountermeasureSystem.h"
#include "FBDatalinkSystem.h"
#include "FBDisplaySystem.h"
#include "FBRadarSystem.h"
#include "FBRwrSystem.h"
#include "FBMasterMode.h"
#include "FBState.h"
#include "FBFdm.h"

namespace FlightBox::World { class FBWorld; }   /* nur geborgt */

namespace FlightBox::Systems {

/* HOTAS (SSC+TQS) + ICP: Stick-/Schalterereignisse nach aktivem Master-Mode geroutet. */
class FBInputSystem {
public:
  virtual ~FBInputSystem() = default;
  virtual void Run(FBMasterMode mode, double dt) { (void)mode; (void)dt; }
};

/* Triebwerks-SYSTEM-Logik ueber dem rohen FDM (F110+DEEC, BINGO/JOKER, EPU) — JSBSims Antriebsmodell
 * treibt den Schub bereits, hier legt sich das Management darueber. */
class FBPropulsionSystem {
public:
  virtual ~FBPropulsionSystem() = default;
  virtual void Run(const Fdm::fb_fdm_state &s, double dt) { (void)s; (void)dt; }
};

/* Historischer SMS/Gun-Stub, ueberholt durch FBStoresSystem/FBGunSystem
 * (doc/flightbox/systems.md, Offene Punkte 1). */
class FBWeaponSystem {
public:
  virtual ~FBWeaponSystem() = default;
  virtual void Run(FBMasterMode mode, const World::FBWorld *world, double dt) { (void)mode; (void)world; (void)dt; }
};

} // namespace FlightBox::Systems
#endif
