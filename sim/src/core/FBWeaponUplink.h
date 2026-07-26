/* FlightBox — the MIDCOURSE UPLINK value types: what a launching aircraft transmits to a missile it is
 * supporting, and what that missile was programmed with on the rail.
 *
 * WHY IT IS A RADIATED SIGNATURE AND NOT A FUNCTION CALL (CLAUDE.md "Kein Cheaten"). The AMRAAM's
 * initial guidance is "datalink command from the launching aircraft ... transitions to onboard active
 * radar terminal homing" (doc/f16/weapons.md §2.5, §4.4). That uplink is a TRANSMISSION: the shooter
 * radiates it, and it stops the moment the shooter stops supporting the shot. So it is published in the
 * launcher's units/FBUnit signature — beside the datalink XMT switch and the IFF transponder, under the
 * same snapshot contract — and the missile READS it through its own comms slot
 * (modules/missile/FBMissileUplink), exactly as a receiver reads any other emission. Nothing hands the
 * missile a pointer to the shooter, and nothing hands either of them the truth.
 *
 * WHAT TRAVELS IN IT IS AN ESTIMATE, NOT A POSITION. FBWeaponTargetState is what the SHOOTER'S RADAR
 * made of the target: a position derived from range/bearing/elevation off its own nose, a velocity
 * differenced from successive looks, and the SIM TIME OF THE LOOK it stands on. The missile therefore
 * flies on data that is as old, as noisy and as wrong as the shooter's sensor picture — which is the
 * whole point of the lost-lock case: when the uplink stops, the last of these is all the missile has. */
#ifndef FBWEAPONUPLINK_H
#define FBWEAPONUPLINK_H

namespace FlightBox {

/* One estimate of where a target is and where it is going, in world terms. Produced by a fire-control
 * system out of its own sensor track; consumed by a weapon. No identity: the shooter's radar does not
 * know who it is looking at either (core/FBRadarContact.h), so neither can the missile. */
struct FBWeaponTargetState {
  bool   Valid = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;   /* ENU, m/s */
  double StampS = 0.0;   /* sim time of the LOOK this estimate stands on — never the time it was sent */
};

/* What a launcher radiates while it supports a shot. Active goes false the instant the fire control
 * loses the track it was supporting, and the missile then has nothing further to receive — the
 * tactically decisive moment, and the reason this is a published state rather than a stream of
 * messages nobody could observe. */
struct FBWeaponUplink {
  bool Active = false;
  int  LauncherId = 0;      /* whose transmitter this is — a missile only listens to its own launcher */
  FBWeaponTargetState Target;
};

} // namespace FlightBox
#endif
