/* FlightBox — FBF16Gun: WHICH gun this jet carries and WHERE it is bolted, nothing else — all behaviour
 * is systems/FBGunSystem. The three offsets are [SET] within the model's stated geometry (no source
 * documents gun installation coordinates), and their SIZE is the point: metres of muzzle offset are
 * fractions of a milliradian at gun range. BORE ALIGNMENT is parallel to the fuselage reference line,
 * no depression: no source gives the angle, and inventing one would bias every burst ever fired.
 * Derivation: doc/flightbox/modules-f16.md §9.2. */
#ifndef FBF16GUN_H
#define FBF16GUN_H

#include "FBGunSystem.h"

namespace FlightBox::Modules {

class FBF16Gun : public Weapons::FBGunSystem {
public:
  FBF16Gun();
};

} // namespace FlightBox::Modules
#endif
