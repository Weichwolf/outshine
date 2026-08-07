/* The MIDCOURSE UPLINK value types. It is a RADIATED SIGNATURE, not a function call: the shooter
 * publishes it in its unit signature and the missile READS it through its own comms slot — nothing
 * hands the missile a pointer to the shooter, and nothing hands either of them the truth.
 * What travels is an ESTIMATE, as old and as wrong as the shooter's sensor picture.
 * doc/core.md, Abschnitt 7.7. */
#ifndef WEAPONUPLINK_H
#define WEAPONUPLINK_H

namespace outshine {

/* No identity: the shooter's radar does not know who it is looking at either, so neither can the
 * missile. */
struct WeaponTargetState {
  bool   Valid = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;   /* ENU, m/s */
  double StampS = 0.0;   /* sim time of the LOOK this estimate stands on — never the time it was sent */
};

/* Active goes false the instant the fire control loses the track it was supporting — the tactically
 * decisive moment, and why this is a published STATE rather than unobservable messages. */
struct WeaponUplink {
  bool Active = false;
  int  LauncherId = 0;      /* whose transmitter this is — a missile only listens to its own launcher */
  WeaponTargetState Target;
};

} // namespace outshine
#endif
