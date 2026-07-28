/* FlightBox — FBNetLinkSystem: the CONTROLLER's feed, and one derivation for one reason.
 *
 * doc/air-defence-network.md §2 offered it as "design B", deferred with the words *it becomes right the
 * day an aircraft joins a control net*. That day is doc/modules/air/module.md §Spec 7: an airborne
 * early-warning node reports to fighters, and a fighter's FBDatalinkBlock is ALREADY OCCUPIED by the
 * Link-16 PPLI that doc/formation.md's station keeping, sort and cover deferral read. One block, one
 * writer — so a second terminal needs a second block, and that is the entire content of this class.
 *
 * IT ADDS NO REGISTRY READER. The scan, the radio horizon, the hold law, the jamming test and the
 * telemetry are all the base's; this file names no unit registry and includes nothing new, so
 * tools/verify_layers.py's PERCEPTION_READERS list stays at SIX entries. That is the acceptance
 * criterion of the round it was built in, and it is checkable rather than promised. */
#ifndef FBNETLINKSYSTEM_H
#define FBNETLINKSYSTEM_H

#include "FBDatalinkSystem.h"

namespace FlightBox::Sensors {

class FBNetLinkSystem : public FBDatalinkSystem {
public:
  /* Its own telemetry name, so a fighter carrying BOTH terminals produces two readable column groups
   * instead of two called `dl_*`. */
  const char *TelemetryName() const override { return "gci"; }

protected:
  FBDatalinkBlock &Block(FBState &state) const override { return state.NetLink; }
};

} // namespace FlightBox::Sensors
#endif
