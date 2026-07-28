/* FlightBox — FBMig29Gun: WHICH gun this aircraft carries and WHERE it is bolted, nothing else — all
 * behaviour is weapons/FBGunSystem, exactly as modules/f16/FBF16Gun is for the M61A1.
 *
 * The GSh-301 sits in the PORT side of the nose/LERX section, built into the airframe ahead of the
 * cockpit ([DCS-FM p.64], [DCS-EA p.86]). The three offsets are read off the deck's own geometry: the
 * design CG is at station 366 in and the EYEPOINT at 181 in, so the cockpit is 4.70 m forward of the
 * CG and a gun "ahead of the cockpit" is [SET] 5.5 m forward of it. BORE ALIGNMENT is parallel to the
 * fuselage reference line: no source gives an installation angle, and inventing one would bias every
 * burst this aircraft ever fires — the same honest zero the F-16 slot carries. */
#ifndef FBMIG29GUN_H
#define FBMIG29GUN_H

#include "FBGunSystem.h"

namespace FlightBox::Modules {

class FBMig29Gun : public Weapons::FBGunSystem {
public:
  FBMig29Gun();
};

} // namespace FlightBox::Modules
#endif
