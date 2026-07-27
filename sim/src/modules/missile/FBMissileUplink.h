/* FlightBox — FBMissileUplink: the missile's COMMS slot, receiving midcourse updates from the aircraft
 * that launched it. It walks the registry for the ONE unit whose id matches its programmed launcher and
 * takes that unit's published uplink emission — nothing else about it, and nothing at all about the
 * target: the content is the SHOOTER'S estimate, with the shooter's errors and the shooter's age.
 * Published as the single datalink TRACK, because that is what the message is; ANONYMOUS, because the
 * shooter's radar does not know who it is looking at either. Losing it is not an error path — the
 * class simply stops publishing and the guidance sees the age grow.
 * doc/flightbox/weapons-and-damage.md §10.2. */
#ifndef FBMISSILEUPLINK_H
#define FBMISSILEUPLINK_H

#include "FBDatalinkSystem.h"

namespace FlightBox {

class FBMissileUplink : public FBDatalinkSystem {
public:
  /* 0 = none, and then nothing is ever received. */
  void SetLauncherId(int id) { LauncherId_ = id; }

  void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS) override;

private:
  int LauncherId_ = 0;
};

} // namespace FlightBox
#endif
