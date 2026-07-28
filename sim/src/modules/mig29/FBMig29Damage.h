/* FlightBox — the MiG-29's DAMAGE LAYOUT: where this airframe's systems sit and how much energy each
 * takes to lose. Module data, exactly as modules/f16/FBF16Damage — the core resolves a burst, the
 * aircraft says which box sits where. Every zone boundary is read out of the FlightBox MiG-29 deck's
 * own structural frame (assets/aircraft/mig29/mig29.xml, "origin at the NOSE TIP, x aft"); the four
 * fragility classes are TAKEN OVER unchanged from the F-16's and are [SET] there as here. */
#ifndef FBMIG29DAMAGE_H
#define FBMIG29DAMAGE_H

#include "FBDamageModel.h"

namespace FlightBox::Modules {

const FBDamageLayout &FBMig29DamageLayout();

} // namespace FlightBox::Modules
#endif
