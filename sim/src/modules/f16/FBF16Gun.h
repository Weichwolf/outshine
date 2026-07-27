/* FlightBox — FBF16Gun: the F-16's internal gun. Everything the gun DOES is the generic
 * systems/FBGunSystem (the drum's books, the interlocks, the trigger command, the burst stream); what is
 * F-16 about this class is the two things that genuinely are — WHICH gun this jet carries and WHERE it
 * is bolted. Exactly the FBF16Sms pattern, and for the same reason: the behaviour is airframe-agnostic,
 * the installation is not.
 *
 * THE INSTALLATION, and how honest it is. The M61A1 sits in the LEFT wing root strake of an F-16, with
 * its muzzle port ahead of and above the cockpit sill on the port side. Numbers, all in the aircraft's
 * own body frame (+fwd/+right/+down from the CG, metres):
 *   Fwd  +4.6   the muzzle port sits forward of the CG, roughly abreast the canopy bow. Derived from the
 *               pinned f16.xml's own geometry rather than picked: the model puts its CG station at
 *               FS -193 in and its nose at the forward end of a 49 ft 5 in fuselage, so the port — which
 *               on the real jet is just ahead of the windscreen — lands ~180 in ahead of the CG.
 *   Right -0.9  port side, half a fuselage width out. The strake installation is on the LEFT, which is
 *               why this number is negative and not zero, and it is worth a metre of lateral offset at
 *               the muzzle.
 *   Down  -0.3  slightly above the reference line: the port is at the top of the strake fillet.
 * All three are [SET] within the geometry the model states. doc/f16/weapons.md documents no gun
 * installation coordinates at all (§4.5 marks even the station data as T4), so these are minimal
 * assumptions consistent with the airframe rather than citations — and their SIZE is what matters: they
 * move the muzzle by metres, which at 600–3,000 ft of gun range is fractions of a milliradian.
 *
 * BORE ALIGNMENT: parallel to the fuselage reference line, no depression, no cant. The real gun is
 * boresighted to a small depression that the sighting system compensates for, but no source in doc/f16/
 * gives the angle, and inventing one would bias every burst this simulator ever fires. Zero is the
 * honest choice and it is stated here rather than left as an omission. */
#ifndef FBF16GUN_H
#define FBF16GUN_H

#include "FBGunSystem.h"

namespace FlightBox {

class FBF16Gun : public FBGunSystem {
public:
  FBF16Gun();
};

} // namespace FlightBox
#endif
