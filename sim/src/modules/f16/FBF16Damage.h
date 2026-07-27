/* FlightBox — the F-16's DAMAGE LAYOUT: where this airframe's systems sit and how much energy each
 * takes to lose. Module data — the core resolves a burst, but which box is in the radome and which in
 * the tail is knowledge only the aircraft has. Every zone boundary is read out of the pinned f16.xml's
 * own structural frame; which system sits where is the placement any photograph shows and no
 * measurement is claimed for it. Boundaries, systems and the four fragility classes with their
 * derivation: doc/flightbox/modules-f16.md §10. */
#ifndef FBF16DAMAGE_H
#define FBF16DAMAGE_H

#include "FBDamageModel.h"

namespace FlightBox::Modules {

const FBDamageLayout &FBF16DamageLayout();

} // namespace FlightBox::Modules
#endif
