/* FlightBox — FBMissileUplink: the missile's COMMS slot, filled with the one thing a missile's comms
 * does — receive midcourse guidance updates from the aircraft that launched it (doc/f16/weapons.md
 * §2.5: "initial guidance = datalink command from the launching aircraft"). It derives from
 * systems/FBDatalinkSystem and overrides its Run for the same reason FBMissileSeeker derives from
 * FBRadarSystem: the slot, the registry access rule and the "publish into your own FBState" contract are
 * exactly right, only the message is different.
 *
 * WHAT IT LISTENS TO. The launcher publishes its guidance transmission in its own units/FBUnit
 * SIGNATURE (core/FBWeaponUplink.h) — an emission, beside its datalink XMT switch and its IFF
 * transponder, under the same snapshot contract. This class walks the registry, finds the ONE unit whose
 * id matches the launcher this round was programmed with, and takes the uplink only if that unit is
 * still radiating one. It reads nothing else about that unit, and nothing at all about the target: the
 * uplink's content is the SHOOTER'S OWN RADAR ESTIMATE, with the shooter's errors and the shooter's age.
 *
 * WHY IT PUBLISHES AS A DATALINK TRACK. The received message IS a cooperative track — a position, a
 * velocity and the time it was measured — so it goes into the datalink block the bus already carries
 * (core/FBAvionicsBlocks.h), as the single entry Tracks[0], and the guidance reads it as an instrument
 * like everything else. No new bus block, no back channel, and the block's own head answers the only
 * question the guidance actually has: is anyone still telling me anything?
 *
 * ANONYMOUS. The track carries the callsign "UPLINK" and no unit id, because the shooter's radar does
 * not know who it is looking at either (core/FBRadarContact.h). A missile cannot learn an identity its
 * launcher never had.
 *
 * LOSING IT IS NOT AN ERROR PATH. When the launcher's fire control drops the lock, Uplink.Active goes
 * false and this class simply stops publishing tracks. The guidance sees the age grow and falls back to
 * inertial. Nothing here decides anything about the flight. */
#ifndef FBMISSILEUPLINK_H
#define FBMISSILEUPLINK_H

#include "FBDatalinkSystem.h"

namespace FlightBox {

class FBMissileUplink : public FBDatalinkSystem {
public:
  /* Which launcher this receiver is tuned to (the round's launch programming). 0 = none, and then
   * nothing is ever received. */
  void SetLauncherId(int id) { LauncherId_ = id; }

  void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS) override;

private:
  int LauncherId_ = 0;
};

} // namespace FlightBox
#endif
